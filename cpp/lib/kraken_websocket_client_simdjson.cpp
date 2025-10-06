#include "kraken_websocket_client_simdjson.hpp"
#include <iostream>
#include <simdjson.h>

using namespace simdjson;

namespace kraken {

KrakenWebSocketClientSimdjson::KrakenWebSocketClientSimdjson()
    : running_(false), connected_(false) {
}

KrakenWebSocketClientSimdjson::~KrakenWebSocketClientSimdjson() {
    stop();
}

bool KrakenWebSocketClientSimdjson::start(const std::vector<std::string>& symbols) {
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

    std::cout << "WebSocket client started (simdjson version)" << std::endl;
    return true;
}

void KrakenWebSocketClientSimdjson::stop() {
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

bool KrakenWebSocketClientSimdjson::is_connected() const {
    return connected_.load();
}

bool KrakenWebSocketClientSimdjson::is_running() const {
    return running_.load();
}

std::vector<TickerRecord> KrakenWebSocketClientSimdjson::get_updates() {
    std::lock_guard<std::mutex> lock(data_mutex_);
    std::vector<TickerRecord> updates = std::move(pending_updates_);
    pending_updates_.clear();
    return updates;
}

std::vector<TickerRecord> KrakenWebSocketClientSimdjson::get_history() const {
    std::lock_guard<std::mutex> lock(data_mutex_);
    return ticker_history_;
}

size_t KrakenWebSocketClientSimdjson::pending_count() const {
    std::lock_guard<std::mutex> lock(data_mutex_);
    return pending_updates_.size();
}

void KrakenWebSocketClientSimdjson::set_update_callback(UpdateCallback callback) {
    std::lock_guard<std::mutex> lock(callback_mutex_);
    update_callback_ = callback;
}

void KrakenWebSocketClientSimdjson::set_connection_callback(ConnectionCallback callback) {
    std::lock_guard<std::mutex> lock(callback_mutex_);
    connection_callback_ = callback;
}

void KrakenWebSocketClientSimdjson::set_error_callback(ErrorCallback callback) {
    std::lock_guard<std::mutex> lock(callback_mutex_);
    error_callback_ = callback;
}

void KrakenWebSocketClientSimdjson::save_to_csv(const std::string& filename) {
    std::lock_guard<std::mutex> lock(data_mutex_);
    Utils::save_to_csv(filename, ticker_history_);
}

// Private methods

KrakenWebSocketClientSimdjson::context_ptr KrakenWebSocketClientSimdjson::on_tls_init(
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

void KrakenWebSocketClientSimdjson::on_open(websocketpp::connection_hdl hdl) {
    std::cout << "WebSocket connection opened" << std::endl;
    connection_hdl_ = hdl;
    connected_ = true;

    notify_connection(true);

    // Build subscription message manually (simdjson is read-only)
    // We'll use simple string building for subscription
    std::ostringstream subscribe_msg;
    subscribe_msg << R"({"method":"subscribe","params":{)";
    subscribe_msg << R"("channel":"ticker",)";
    subscribe_msg << R"("symbol":[)";

    for (size_t i = 0; i < symbols_.size(); ++i) {
        if (i > 0) subscribe_msg << ",";
        subscribe_msg << "\"" << symbols_[i] << "\"";
    }

    subscribe_msg << R"(],"snapshot":true}})";

    std::string msg_str = subscribe_msg.str();
    std::cout << "Subscribing to: " << msg_str << std::endl;

    try {
        ws_client_.send(hdl, msg_str, websocketpp::frame::opcode::text);
    } catch (const std::exception& e) {
        notify_error("Send error: " + std::string(e.what()));
    }
}

void KrakenWebSocketClientSimdjson::on_close(websocketpp::connection_hdl) {
    std::cout << "WebSocket connection closed" << std::endl;
    connected_ = false;
    notify_connection(false);
}

void KrakenWebSocketClientSimdjson::on_fail(websocketpp::connection_hdl) {
    std::cerr << "WebSocket connection failed" << std::endl;
    connected_ = false;
    notify_connection(false);
    notify_error("Connection failed");
}

void KrakenWebSocketClientSimdjson::on_message(websocketpp::connection_hdl,
                                               client::message_ptr msg) {
    try {
        // simdjson parsing - on-demand API for zero-copy parsing
        ondemand::parser parser;

        // Get message payload
        const std::string& payload = msg->get_payload();

        // Pad the string (simdjson requirement)
        padded_string padded(payload);

        // Parse document (zero-copy, lazy parsing)
        ondemand::document doc = parser.iterate(padded);

        // Handle subscription status
        if (auto method_result = doc["method"]; !method_result.error()) {
            std::string_view method = method_result.value();
            if (method == "subscribe") {
                // Check success field
                if (auto success_result = doc["success"]; !success_result.error()) {
                    bool success = success_result.value();
                    if (success) {
                        std::cout << "Successfully subscribed (simdjson)" << std::endl;
                    } else {
                        notify_error("Subscription failed");
                    }
                }
                return;
            }
        }

        // Handle heartbeat
        if (auto channel_result = doc["channel"]; !channel_result.error()) {
            std::string_view channel = channel_result.value();
            if (channel == "heartbeat") {
                return;
            }

            // Handle ticker messages
            if (channel == "ticker") {
                // Check type field
                auto type_result = doc["type"];
                if (type_result.error()) return;

                std::string_view type_str = type_result.value();
                if (type_str != "snapshot" && type_str != "update") return;

                std::string timestamp = Utils::get_utc_timestamp();

                // Parse data array
                auto data_result = doc["data"];
                if (data_result.error()) return;

                ondemand::array data_array = data_result.value();

                for (auto ticker_value : data_array) {
                    ondemand::object ticker = ticker_value.get_object();

                    TickerRecord record;
                    record.timestamp = timestamp;

                    // Extract string fields (convert string_view to string)
                    if (auto symbol = ticker["symbol"]; !symbol.error()) {
                        std::string_view sv = symbol.value();
                        record.pair = std::string(sv);
                    }

                    record.type = std::string(type_str);

                    // Extract double fields (simdjson API)
                    // Use .get_double() instead of .get<double>()
                    if (auto bid = ticker["bid"]; !bid.error()) {
                        record.bid = bid.get_double();
                    }
                    if (auto bid_qty = ticker["bid_qty"]; !bid_qty.error()) {
                        record.bid_qty = bid_qty.get_double();
                    }
                    if (auto ask = ticker["ask"]; !ask.error()) {
                        record.ask = ask.get_double();
                    }
                    if (auto ask_qty = ticker["ask_qty"]; !ask_qty.error()) {
                        record.ask_qty = ask_qty.get_double();
                    }
                    if (auto last = ticker["last"]; !last.error()) {
                        record.last = last.get_double();
                    }
                    if (auto volume = ticker["volume"]; !volume.error()) {
                        record.volume = volume.get_double();
                    }
                    if (auto vwap = ticker["vwap"]; !vwap.error()) {
                        record.vwap = vwap.get_double();
                    }
                    if (auto low = ticker["low"]; !low.error()) {
                        record.low = low.get_double();
                    }
                    if (auto high = ticker["high"]; !high.error()) {
                        record.high = high.get_double();
                    }
                    if (auto change = ticker["change"]; !change.error()) {
                        record.change = change.get_double();
                    }
                    if (auto change_pct = ticker["change_pct"]; !change_pct.error()) {
                        record.change_pct = change_pct.get_double();
                    }

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

    } catch (const simdjson_error& e) {
        notify_error("simdjson parsing error: " + std::string(error_message(e.error())));
    } catch (const std::exception& e) {
        notify_error("Message handling error: " + std::string(e.what()));
    }
}

void KrakenWebSocketClientSimdjson::run_client() {
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

void KrakenWebSocketClientSimdjson::notify_connection(bool connected) {
    std::lock_guard<std::mutex> lock(callback_mutex_);
    if (connection_callback_) {
        connection_callback_(connected);
    }
}

void KrakenWebSocketClientSimdjson::notify_error(const std::string& error) {
    std::cerr << "[Error] " << error << std::endl;

    std::lock_guard<std::mutex> lock(callback_mutex_);
    if (error_callback_) {
        error_callback_(error);
    }
}

} // namespace kraken
