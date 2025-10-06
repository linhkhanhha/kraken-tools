#ifndef KRAKEN_WEBSOCKET_CLIENT_BASE_HPP
#define KRAKEN_WEBSOCKET_CLIENT_BASE_HPP

#include <string>
#include <vector>
#include <thread>
#include <mutex>
#include <atomic>
#include <functional>
#include <websocketpp/config/asio_client.hpp>
#include <websocketpp/client.hpp>
#include "kraken_common.hpp"

namespace kraken {

/**
 * Base template class for WebSocket clients with different JSON parsers
 *
 * Template parameter JsonParser must provide:
 * - static std::string build_subscription(const std::vector<std::string>& symbols)
 * - static void parse_message(const std::string& payload,
 *                             std::function<void(const TickerRecord&)> callback)
 *
 * This eliminates code duplication between nlohmann and simdjson implementations
 */
template<typename JsonParser>
class KrakenWebSocketClientBase {
public:
    // Type definitions
    using UpdateCallback = std::function<void(const TickerRecord&)>;
    using ConnectionCallback = std::function<void(bool connected)>;
    using ErrorCallback = std::function<void(const std::string& error)>;

    KrakenWebSocketClientBase();
    virtual ~KrakenWebSocketClientBase();

    // Disable copy
    KrakenWebSocketClientBase(const KrakenWebSocketClientBase&) = delete;
    KrakenWebSocketClientBase& operator=(const KrakenWebSocketClientBase&) = delete;

    // Public API
    bool start(const std::vector<std::string>& symbols);
    void stop();
    bool is_connected() const;
    bool is_running() const;
    std::vector<TickerRecord> get_updates();
    std::vector<TickerRecord> get_history() const;
    size_t pending_count() const;
    void set_update_callback(UpdateCallback callback);
    void set_connection_callback(ConnectionCallback callback);
    void set_error_callback(ErrorCallback callback);
    void save_to_csv(const std::string& filename);

protected:
    // WebSocket types
    typedef websocketpp::client<websocketpp::config::asio_tls_client> client;
    typedef websocketpp::lib::shared_ptr<websocketpp::lib::asio::ssl::context> context_ptr;

    // WebSocket client and connection
    client ws_client_;
    websocketpp::connection_hdl connection_hdl_;
    std::thread worker_thread_;

    // State
    std::atomic<bool> running_;
    std::atomic<bool> connected_;
    std::vector<std::string> symbols_;

    // Data storage (protected by data_mutex_)
    mutable std::mutex data_mutex_;
    std::vector<TickerRecord> ticker_history_;
    std::vector<TickerRecord> pending_updates_;

    // Callbacks (protected by callback_mutex_)
    mutable std::mutex callback_mutex_;
    UpdateCallback update_callback_;
    ConnectionCallback connection_callback_;
    ErrorCallback error_callback_;

    // WebSocket event handlers
    context_ptr on_tls_init(websocketpp::connection_hdl hdl);
    void on_open(websocketpp::connection_hdl hdl);
    void on_close(websocketpp::connection_hdl hdl);
    void on_fail(websocketpp::connection_hdl hdl);
    void on_message(websocketpp::connection_hdl hdl, client::message_ptr msg);

    // Worker thread main function
    void run_client();

    // Helper methods
    void notify_connection(bool connected);
    void notify_error(const std::string& error);
    void add_record(const TickerRecord& record);
};

// Implementation must be in header for templates

template<typename JsonParser>
KrakenWebSocketClientBase<JsonParser>::KrakenWebSocketClientBase()
    : running_(false), connected_(false) {
}

template<typename JsonParser>
KrakenWebSocketClientBase<JsonParser>::~KrakenWebSocketClientBase() {
    stop();
}

template<typename JsonParser>
bool KrakenWebSocketClientBase<JsonParser>::start(const std::vector<std::string>& symbols) {
    if (running_) {
        std::cerr << "Client already running" << std::endl;
        return false;
    }

    if (symbols.empty()) {
        std::cerr << "No symbols provided" << std::endl;
        return false;
    }

    symbols_ = symbols;
    running_ = true;

    worker_thread_ = std::thread([this]() {
        this->run_client();
    });

    std::cout << "WebSocket client started (" << JsonParser::name() << " version)" << std::endl;
    return true;
}

template<typename JsonParser>
void KrakenWebSocketClientBase<JsonParser>::stop() {
    if (!running_) {
        return;
    }

    running_ = false;
    connected_ = false;

    try {
        if (!ws_client_.stopped()) {
            ws_client_.stop();
        }

        if (worker_thread_.joinable()) {
            worker_thread_.join();
        }

        std::cout << "WebSocket client stopped" << std::endl;
    } catch (const std::exception& e) {
        std::cerr << "Error stopping client: " << e.what() << std::endl;
    }
}

template<typename JsonParser>
bool KrakenWebSocketClientBase<JsonParser>::is_connected() const {
    return connected_.load();
}

template<typename JsonParser>
bool KrakenWebSocketClientBase<JsonParser>::is_running() const {
    return running_.load();
}

template<typename JsonParser>
std::vector<TickerRecord> KrakenWebSocketClientBase<JsonParser>::get_updates() {
    std::lock_guard<std::mutex> lock(data_mutex_);
    std::vector<TickerRecord> updates = std::move(pending_updates_);
    pending_updates_.clear();
    return updates;
}

template<typename JsonParser>
std::vector<TickerRecord> KrakenWebSocketClientBase<JsonParser>::get_history() const {
    std::lock_guard<std::mutex> lock(data_mutex_);
    return ticker_history_;
}

template<typename JsonParser>
size_t KrakenWebSocketClientBase<JsonParser>::pending_count() const {
    std::lock_guard<std::mutex> lock(data_mutex_);
    return pending_updates_.size();
}

template<typename JsonParser>
void KrakenWebSocketClientBase<JsonParser>::set_update_callback(UpdateCallback callback) {
    std::lock_guard<std::mutex> lock(callback_mutex_);
    update_callback_ = callback;
}

template<typename JsonParser>
void KrakenWebSocketClientBase<JsonParser>::set_connection_callback(ConnectionCallback callback) {
    std::lock_guard<std::mutex> lock(callback_mutex_);
    connection_callback_ = callback;
}

template<typename JsonParser>
void KrakenWebSocketClientBase<JsonParser>::set_error_callback(ErrorCallback callback) {
    std::lock_guard<std::mutex> lock(callback_mutex_);
    error_callback_ = callback;
}

template<typename JsonParser>
void KrakenWebSocketClientBase<JsonParser>::save_to_csv(const std::string& filename) {
    std::lock_guard<std::mutex> lock(data_mutex_);
    Utils::save_to_csv(filename, ticker_history_);
}

template<typename JsonParser>
typename KrakenWebSocketClientBase<JsonParser>::context_ptr
KrakenWebSocketClientBase<JsonParser>::on_tls_init(websocketpp::connection_hdl) {
    context_ptr ctx = std::make_shared<boost::asio::ssl::context>(
        boost::asio::ssl::context::sslv23);

    try {
        ctx->set_options(boost::asio::ssl::context::default_workarounds |
                        boost::asio::ssl::context::no_sslv2 |
                        boost::asio::ssl::context::no_sslv3 |
                        boost::asio::ssl::context::single_dh_use);
    } catch (std::exception& e) {
        notify_error("TLS init error: " + std::string(e.what()));
    }

    return ctx;
}

template<typename JsonParser>
void KrakenWebSocketClientBase<JsonParser>::on_open(websocketpp::connection_hdl hdl) {
    std::cout << "WebSocket connection opened" << std::endl;
    connection_hdl_ = hdl;
    connected_ = true;

    notify_connection(true);

    // Build subscription using parser-specific method
    std::string msg_str = JsonParser::build_subscription(symbols_);
    std::cout << "Subscribing to: " << msg_str << std::endl;

    try {
        ws_client_.send(hdl, msg_str, websocketpp::frame::opcode::text);
    } catch (const std::exception& e) {
        notify_error("Send error: " + std::string(e.what()));
    }
}

template<typename JsonParser>
void KrakenWebSocketClientBase<JsonParser>::on_close(websocketpp::connection_hdl) {
    std::cout << "WebSocket connection closed" << std::endl;
    connected_ = false;
    notify_connection(false);
}

template<typename JsonParser>
void KrakenWebSocketClientBase<JsonParser>::on_fail(websocketpp::connection_hdl) {
    std::cerr << "WebSocket connection failed" << std::endl;
    connected_ = false;
    notify_connection(false);
    notify_error("Connection failed");
}

template<typename JsonParser>
void KrakenWebSocketClientBase<JsonParser>::on_message(
    websocketpp::connection_hdl, client::message_ptr msg) {

    try {
        // Use parser-specific parsing - it will call add_record() for each ticker
        JsonParser::parse_message(msg->get_payload(),
            [this](const TickerRecord& record) {
                this->add_record(record);
            });
    } catch (const std::exception& e) {
        notify_error("Message handling error: " + std::string(e.what()));
    }
}

template<typename JsonParser>
void KrakenWebSocketClientBase<JsonParser>::run_client() {
    try {
        ws_client_.init_asio();
        ws_client_.set_tls_init_handler([this](websocketpp::connection_hdl hdl) {
            return this->on_tls_init(hdl);
        });

        ws_client_.set_open_handler([this](websocketpp::connection_hdl hdl) {
            this->on_open(hdl);
        });
        ws_client_.set_message_handler([this](websocketpp::connection_hdl hdl, client::message_ptr msg) {
            this->on_message(hdl, msg);
        });
        ws_client_.set_close_handler([this](websocketpp::connection_hdl hdl) {
            this->on_close(hdl);
        });
        ws_client_.set_fail_handler([this](websocketpp::connection_hdl hdl) {
            this->on_fail(hdl);
        });

        // Set logging
        ws_client_.clear_access_channels(websocketpp::log::alevel::all);
        ws_client_.set_access_channels(websocketpp::log::alevel::connect);
        ws_client_.set_access_channels(websocketpp::log::alevel::disconnect);

        // Connect
        std::string uri = "wss://ws.kraken.com/v2";
        websocketpp::lib::error_code ec;
        client::connection_ptr con = ws_client_.get_connection(uri, ec);

        if (ec) {
            notify_error("Connection error: " + ec.message());
            running_ = false;
            return;
        }

        ws_client_.connect(con);
        std::cout << "Connecting to " << uri << "..." << std::endl;

        ws_client_.run();

    } catch (const websocketpp::exception& e) {
        notify_error("WebSocket++ exception: " + std::string(e.what()));
    } catch (const std::exception& e) {
        notify_error("Exception: " + std::string(e.what()));
    }

    running_ = false;
    connected_ = false;
}

template<typename JsonParser>
void KrakenWebSocketClientBase<JsonParser>::notify_connection(bool connected) {
    std::lock_guard<std::mutex> lock(callback_mutex_);
    if (connection_callback_) {
        connection_callback_(connected);
    }
}

template<typename JsonParser>
void KrakenWebSocketClientBase<JsonParser>::notify_error(const std::string& error) {
    std::cerr << "[Error] " << error << std::endl;

    std::lock_guard<std::mutex> lock(callback_mutex_);
    if (error_callback_) {
        error_callback_(error);
    }
}

template<typename JsonParser>
void KrakenWebSocketClientBase<JsonParser>::add_record(const TickerRecord& record) {
    // Store in history and pending
    {
        std::lock_guard<std::mutex> lock(data_mutex_);
        ticker_history_.push_back(record);
        pending_updates_.push_back(record);
    }

    // Call user callback (outside data lock)
    {
        std::lock_guard<std::mutex> lock(callback_mutex_);
        if (update_callback_) {
            update_callback_(record);
        }
    }
}

} // namespace kraken

#endif // KRAKEN_WEBSOCKET_CLIENT_BASE_HPP
