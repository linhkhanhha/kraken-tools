#ifndef KRAKEN_WEBSOCKET_CLIENT_BASE_HYBRID_HPP
#define KRAKEN_WEBSOCKET_CLIENT_BASE_HYBRID_HPP

#include <string>
#include <vector>
#include <thread>
#include <mutex>
#include <atomic>
#include <functional>
#include <type_traits>
#include <websocketpp/config/asio_client.hpp>
#include <websocketpp/client.hpp>
#include "kraken_common.hpp"

namespace kraken {

/**
 * HYBRID APPROACH: High-performance WebSocket client with zero-overhead callbacks
 *
 * Template parameters:
 * - JsonParser: JSON parsing strategy (nlohmann/simdjson)
 * - UpdateCallback: Callback type for high-frequency ticker updates
 *                   Default: std::function for ease of use
 *                   For performance: pass lambda type directly
 *
 * PERFORMANCE OPTIMIZATION:
 * - UpdateCallback uses template (zero overhead when non-capturing lambda)
 * - ConnectionCallback/ErrorCallback use std::function (called rarely)
 *
 * This eliminates virtual call overhead for hot path (ticker updates)
 * while maintaining flexibility for cold path (connection/error events).
 */
template<typename JsonParser,
         typename UpdateCallback = std::function<void(const TickerRecord&)>>
class KrakenWebSocketClientBaseHybrid {
public:
    // Type definitions
    // FAST PATH: Template callback for high-frequency updates (1000s/sec)
    using FastUpdateCallback = UpdateCallback;

    // SLOW PATH: std::function for rare events (<10/session)
    using ConnectionCallback = std::function<void(bool connected)>;
    using ErrorCallback = std::function<void(const std::string& error)>;

    KrakenWebSocketClientBaseHybrid();
    virtual ~KrakenWebSocketClientBaseHybrid();

    // Disable copy
    KrakenWebSocketClientBaseHybrid(const KrakenWebSocketClientBaseHybrid&) = delete;
    KrakenWebSocketClientBaseHybrid& operator=(const KrakenWebSocketClientBaseHybrid&) = delete;

    // Public API
    bool start(const std::vector<std::string>& symbols);
    void stop();
    bool is_connected() const;
    bool is_running() const;
    std::vector<TickerRecord> get_updates();
    std::vector<TickerRecord> get_history() const;
    size_t pending_count() const;

    // HYBRID CALLBACK SETTERS:
    // Fast path: template callback (can be inlined)
    void set_update_callback(FastUpdateCallback callback);

    // Slow path: std::function (flexibility for rare events)
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

    // Callbacks
    // PERFORMANCE NOTE: update_callback_ is NOT behind mutex for zero overhead
    // It should be set before start() and not changed during runtime
    FastUpdateCallback update_callback_;

    // Rare event callbacks (protected by callback_mutex_)
    mutable std::mutex callback_mutex_;
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

template<typename JsonParser, typename UpdateCallback>
KrakenWebSocketClientBaseHybrid<JsonParser, UpdateCallback>::KrakenWebSocketClientBaseHybrid()
    : running_(false), connected_(false) {
}

template<typename JsonParser, typename UpdateCallback>
KrakenWebSocketClientBaseHybrid<JsonParser, UpdateCallback>::~KrakenWebSocketClientBaseHybrid() {
    stop();
}

template<typename JsonParser, typename UpdateCallback>
bool KrakenWebSocketClientBaseHybrid<JsonParser, UpdateCallback>::start(
    const std::vector<std::string>& symbols) {

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

    std::cout << "WebSocket client started (" << JsonParser::name()
              << " version, hybrid callbacks)" << std::endl;
    return true;
}

template<typename JsonParser, typename UpdateCallback>
void KrakenWebSocketClientBaseHybrid<JsonParser, UpdateCallback>::stop() {
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

template<typename JsonParser, typename UpdateCallback>
bool KrakenWebSocketClientBaseHybrid<JsonParser, UpdateCallback>::is_connected() const {
    return connected_.load();
}

template<typename JsonParser, typename UpdateCallback>
bool KrakenWebSocketClientBaseHybrid<JsonParser, UpdateCallback>::is_running() const {
    return running_.load();
}

template<typename JsonParser, typename UpdateCallback>
std::vector<TickerRecord> KrakenWebSocketClientBaseHybrid<JsonParser, UpdateCallback>::get_updates() {
    std::lock_guard<std::mutex> lock(data_mutex_);
    std::vector<TickerRecord> updates = std::move(pending_updates_);
    pending_updates_.clear();
    return updates;
}

template<typename JsonParser, typename UpdateCallback>
std::vector<TickerRecord> KrakenWebSocketClientBaseHybrid<JsonParser, UpdateCallback>::get_history() const {
    std::lock_guard<std::mutex> lock(data_mutex_);
    return ticker_history_;
}

template<typename JsonParser, typename UpdateCallback>
size_t KrakenWebSocketClientBaseHybrid<JsonParser, UpdateCallback>::pending_count() const {
    std::lock_guard<std::mutex> lock(data_mutex_);
    return pending_updates_.size();
}

template<typename JsonParser, typename UpdateCallback>
void KrakenWebSocketClientBaseHybrid<JsonParser, UpdateCallback>::set_update_callback(
    FastUpdateCallback callback) {

    // PERFORMANCE OPTIMIZATION:
    // No mutex lock for update_callback_ - should be set before start()
    // This allows the compiler to inline the callback completely
    update_callback_ = std::move(callback);
}

template<typename JsonParser, typename UpdateCallback>
void KrakenWebSocketClientBaseHybrid<JsonParser, UpdateCallback>::set_connection_callback(
    ConnectionCallback callback) {

    std::lock_guard<std::mutex> lock(callback_mutex_);
    connection_callback_ = callback;
}

template<typename JsonParser, typename UpdateCallback>
void KrakenWebSocketClientBaseHybrid<JsonParser, UpdateCallback>::set_error_callback(
    ErrorCallback callback) {

    std::lock_guard<std::mutex> lock(callback_mutex_);
    error_callback_ = callback;
}

template<typename JsonParser, typename UpdateCallback>
void KrakenWebSocketClientBaseHybrid<JsonParser, UpdateCallback>::save_to_csv(
    const std::string& filename) {

    std::lock_guard<std::mutex> lock(data_mutex_);
    Utils::save_to_csv(filename, ticker_history_);
}

template<typename JsonParser, typename UpdateCallback>
typename KrakenWebSocketClientBaseHybrid<JsonParser, UpdateCallback>::context_ptr
KrakenWebSocketClientBaseHybrid<JsonParser, UpdateCallback>::on_tls_init(
    websocketpp::connection_hdl) {

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

template<typename JsonParser, typename UpdateCallback>
void KrakenWebSocketClientBaseHybrid<JsonParser, UpdateCallback>::on_open(
    websocketpp::connection_hdl hdl) {

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

template<typename JsonParser, typename UpdateCallback>
void KrakenWebSocketClientBaseHybrid<JsonParser, UpdateCallback>::on_close(
    websocketpp::connection_hdl) {

    std::cout << "WebSocket connection closed" << std::endl;
    connected_ = false;
    notify_connection(false);
}

template<typename JsonParser, typename UpdateCallback>
void KrakenWebSocketClientBaseHybrid<JsonParser, UpdateCallback>::on_fail(
    websocketpp::connection_hdl) {

    std::cerr << "WebSocket connection failed" << std::endl;
    connected_ = false;
    notify_connection(false);
    notify_error("Connection failed");
}

template<typename JsonParser, typename UpdateCallback>
void KrakenWebSocketClientBaseHybrid<JsonParser, UpdateCallback>::on_message(
    websocketpp::connection_hdl, client::message_ptr msg) {

    try {
        // CRITICAL PERFORMANCE PATH:
        // The lambda passed to parse_message will be inlined when UpdateCallback
        // is a stateless lambda type (not std::function)
        JsonParser::parse_message(msg->get_payload(),
            [this](const TickerRecord& record) {
                this->add_record(record);
            });
    } catch (const std::exception& e) {
        notify_error("Message handling error: " + std::string(e.what()));
    }
}

template<typename JsonParser, typename UpdateCallback>
void KrakenWebSocketClientBaseHybrid<JsonParser, UpdateCallback>::run_client() {
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

template<typename JsonParser, typename UpdateCallback>
void KrakenWebSocketClientBaseHybrid<JsonParser, UpdateCallback>::notify_connection(bool connected) {
    std::lock_guard<std::mutex> lock(callback_mutex_);
    if (connection_callback_) {
        connection_callback_(connected);
    }
}

template<typename JsonParser, typename UpdateCallback>
void KrakenWebSocketClientBaseHybrid<JsonParser, UpdateCallback>::notify_error(
    const std::string& error) {

    std::cerr << "[Error] " << error << std::endl;

    std::lock_guard<std::mutex> lock(callback_mutex_);
    if (error_callback_) {
        error_callback_(error);
    }
}

template<typename JsonParser, typename UpdateCallback>
void KrakenWebSocketClientBaseHybrid<JsonParser, UpdateCallback>::add_record(
    const TickerRecord& record) {

    // Store in history and pending
    {
        std::lock_guard<std::mutex> lock(data_mutex_);
        ticker_history_.push_back(record);
        pending_updates_.push_back(record);
    }

    // CRITICAL HOT PATH:
    // Call user callback WITHOUT mutex lock for maximum performance
    // When UpdateCallback is a stateless lambda type, this will be fully inlined
    // with zero overhead (no function pointer, no virtual call, no heap allocation)
    if constexpr (std::is_invocable_v<UpdateCallback, const TickerRecord&>) {
        // For non-std::function callbacks, direct call (zero overhead)
        update_callback_(record);
    } else if constexpr (std::is_same_v<UpdateCallback, std::function<void(const TickerRecord&)>>) {
        // For std::function, check if it's set before calling
        if (update_callback_) {
            update_callback_(record);
        }
    }
}

} // namespace kraken

#endif // KRAKEN_WEBSOCKET_CLIENT_BASE_HYBRID_HPP
