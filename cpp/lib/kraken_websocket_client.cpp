#include "kraken_websocket_client.hpp"
#include <iostream>
#include <nlohmann/json.hpp>

using json = nlohmann::json;

namespace kraken {

KrakenWebSocketClient::KrakenWebSocketClient()
    : running_(false), connected_(false) {
}

KrakenWebSocketClient::~KrakenWebSocketClient() {
    stop();
}

bool KrakenWebSocketClient::start(const std::vector<std::string>& symbols) {
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

    // Start worker thread
    worker_thread_ = std::thread([this]() {
        this->run_client();
    });

    std::cout << "WebSocket client started (non-blocking)" << std::endl;
    return true;
}

void KrakenWebSocketClient::stop() {
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

bool KrakenWebSocketClient::is_connected() const {
    return connected_.load();
}

bool KrakenWebSocketClient::is_running() const {
    return running_.load();
}

std::vector<TickerRecord> KrakenWebSocketClient::get_updates() {
    std::lock_guard<std::mutex> lock(data_mutex_);
    std::vector<TickerRecord> updates = std::move(pending_updates_);
    pending_updates_.clear();
    return updates;
}

std::vector<TickerRecord> KrakenWebSocketClient::get_history() const {
    std::lock_guard<std::mutex> lock(data_mutex_);
    return ticker_history_;
}

size_t KrakenWebSocketClient::pending_count() const {
    std::lock_guard<std::mutex> lock(data_mutex_);
    return pending_updates_.size();
}

void KrakenWebSocketClient::set_update_callback(UpdateCallback callback) {
    std::lock_guard<std::mutex> lock(callback_mutex_);
    update_callback_ = callback;
}

void KrakenWebSocketClient::set_connection_callback(ConnectionCallback callback) {
    std::lock_guard<std::mutex> lock(callback_mutex_);
    connection_callback_ = callback;
}

void KrakenWebSocketClient::set_error_callback(ErrorCallback callback) {
    std::lock_guard<std::mutex> lock(callback_mutex_);
    error_callback_ = callback;
}

void KrakenWebSocketClient::save_to_csv(const std::string& filename) {
    std::lock_guard<std::mutex> lock(data_mutex_);
    Utils::save_to_csv(filename, ticker_history_);
}

// Private methods

KrakenWebSocketClient::context_ptr KrakenWebSocketClient::on_tls_init(websocketpp::connection_hdl) {
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

void KrakenWebSocketClient::on_open(websocketpp::connection_hdl hdl) {
    std::cout << "WebSocket connection opened" << std::endl;
    connection_hdl_ = hdl;
    connected_ = true;

    notify_connection(true);

    // Send subscription message
    json subscribe_msg = {
        {"method", "subscribe"},
        {"params", {
            {"channel", "ticker"},
            {"symbol", symbols_},
            {"snapshot", true}
        }}
    };

    std::string msg_str = subscribe_msg.dump();
    std::cout << "Subscribing to: " << msg_str << std::endl;

    try {
        ws_client_.send(hdl, msg_str, websocketpp::frame::opcode::text);
    } catch (const std::exception& e) {
        notify_error("Send error: " + std::string(e.what()));
    }
}

void KrakenWebSocketClient::on_close(websocketpp::connection_hdl) {
    std::cout << "WebSocket connection closed" << std::endl;
    connected_ = false;
    notify_connection(false);
}

void KrakenWebSocketClient::on_fail(websocketpp::connection_hdl) {
    std::cerr << "WebSocket connection failed" << std::endl;
    connected_ = false;
    notify_connection(false);
    notify_error("Connection failed");
}

void KrakenWebSocketClient::on_message(websocketpp::connection_hdl, client::message_ptr msg) {
    try {
        json data = json::parse(msg->get_payload());

        // Handle subscription status
        if (data.contains("method") && data["method"] == "subscribe") {
            if (data.contains("success") && data["success"].get<bool>()) {
                std::cout << "Successfully subscribed" << std::endl;
            } else {
                notify_error("Subscription failed: " + data.dump());
            }
            return;
        }

        // Handle heartbeat
        if (data.contains("channel") && data["channel"] == "heartbeat") {
            return;
        }

        // Handle ticker messages
        if (data.contains("channel") && data["channel"] == "ticker" &&
            data.contains("type") && (data["type"] == "snapshot" || data["type"] == "update")) {

            std::string timestamp = Utils::get_utc_timestamp();

            if (data.contains("data") && data["data"].is_array()) {
                for (const auto& ticker_data : data["data"]) {
                    TickerRecord record;
                    record.timestamp = timestamp;
                    record.pair = ticker_data.value("symbol", "");
                    record.type = data.value("type", "");
                    record.bid = ticker_data.value("bid", 0.0);
                    record.bid_qty = ticker_data.value("bid_qty", 0.0);
                    record.ask = ticker_data.value("ask", 0.0);
                    record.ask_qty = ticker_data.value("ask_qty", 0.0);
                    record.last = ticker_data.value("last", 0.0);
                    record.volume = ticker_data.value("volume", 0.0);
                    record.vwap = ticker_data.value("vwap", 0.0);
                    record.low = ticker_data.value("low", 0.0);
                    record.high = ticker_data.value("high", 0.0);
                    record.change = ticker_data.value("change", 0.0);
                    record.change_pct = ticker_data.value("change_pct", 0.0);

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
            }
        }

    } catch (const json::exception& e) {
        notify_error("JSON parsing error: " + std::string(e.what()));
    } catch (const std::exception& e) {
        notify_error("Message handling error: " + std::string(e.what()));
    }
}

void KrakenWebSocketClient::run_client() {
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

        // Run event loop (blocks until stopped)
        ws_client_.run();

    } catch (const websocketpp::exception& e) {
        notify_error("WebSocket++ exception: " + std::string(e.what()));
    } catch (const std::exception& e) {
        notify_error("Exception: " + std::string(e.what()));
    }

    running_ = false;
    connected_ = false;
}

void KrakenWebSocketClient::notify_connection(bool connected) {
    std::lock_guard<std::mutex> lock(callback_mutex_);
    if (connection_callback_) {
        connection_callback_(connected);
    }
}

void KrakenWebSocketClient::notify_error(const std::string& error) {
    std::cerr << "[Error] " << error << std::endl;

    std::lock_guard<std::mutex> lock(callback_mutex_);
    if (error_callback_) {
        error_callback_(error);
    }
}

} // namespace kraken
