/**
 * Kraken WebSocket Client for Order Book Data (Level 2)
 *
 * Subscribes to Kraken WebSocket v2 book channel and processes
 * order book snapshots and updates.
 */

#ifndef KRAKEN_BOOK_CLIENT_HPP
#define KRAKEN_BOOK_CLIENT_HPP

#include <string>
#include <vector>
#include <thread>
#include <mutex>
#include <atomic>
#include <functional>
#include <sstream>
#include <iostream>
#include <map>
#include <websocketpp/config/asio_client.hpp>
#include <websocketpp/client.hpp>
#include <simdjson.h>
#include "orderbook_common.hpp"
#include "jsonl_writer.hpp"
#include "kraken_common.hpp"

namespace kraken {

/**
 * WebSocket client for order book (Level 2) data
 */
class KrakenBookClient {
public:
    // Type definitions
    using UpdateCallback = std::function<void(const OrderBookRecord&)>;
    using ConnectionCallback = std::function<void(bool connected)>;
    using ErrorCallback = std::function<void(const std::string& error)>;

    KrakenBookClient(int depth = 10, bool validate_checksums = true);
    ~KrakenBookClient();

    // Disable copy
    KrakenBookClient(const KrakenBookClient&) = delete;
    KrakenBookClient& operator=(const KrakenBookClient&) = delete;

    // Public API
    bool start(const std::vector<std::string>& symbols);
    void stop();
    bool is_connected() const;
    bool is_running() const;
    void set_update_callback(UpdateCallback callback);
    void set_connection_callback(ConnectionCallback callback);
    void set_error_callback(ErrorCallback callback);

    // Get statistics per symbol
    std::map<std::string, OrderBookStats> get_stats() const;

private:
    // WebSocket types
    typedef websocketpp::client<websocketpp::config::asio_tls_client> client;
    typedef websocketpp::lib::shared_ptr<websocketpp::lib::asio::ssl::context> context_ptr;

    // Configuration
    int depth_;
    bool validate_checksums_;

    // WebSocket client and connection
    client ws_client_;
    websocketpp::connection_hdl connection_hdl_;
    std::thread worker_thread_;

    // State
    std::atomic<bool> running_;
    std::atomic<bool> connected_;
    std::vector<std::string> symbols_;

    // Statistics (protected by stats_mutex_)
    mutable std::mutex stats_mutex_;
    std::map<std::string, OrderBookStats> stats_;

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
    void process_book_message(const std::string& payload);
    std::string build_subscription() const;
};

// ============================================================================
// Implementation
// ============================================================================

KrakenBookClient::KrakenBookClient(int depth, bool validate_checksums)
    : depth_(depth), validate_checksums_(validate_checksums),
      running_(false), connected_(false) {

    // Initialize WebSocket client
    ws_client_.clear_access_channels(websocketpp::log::alevel::all);
    ws_client_.clear_error_channels(websocketpp::log::elevel::all);
    ws_client_.init_asio();
    ws_client_.set_tls_init_handler(std::bind(
        &KrakenBookClient::on_tls_init, this, std::placeholders::_1
    ));
}

KrakenBookClient::~KrakenBookClient() {
    stop();
}

bool KrakenBookClient::start(const std::vector<std::string>& symbols) {
    if (running_) {
        return false;
    }

    symbols_ = symbols;
    running_ = true;

    // Initialize statistics
    {
        std::lock_guard<std::mutex> lock(stats_mutex_);
        stats_.clear();
        for (const auto& symbol : symbols_) {
            stats_[symbol] = OrderBookStats();
        }
    }

    // Start worker thread
    worker_thread_ = std::thread(&KrakenBookClient::run_client, this);

    return true;
}

void KrakenBookClient::stop() {
    if (!running_) {
        return;
    }

    running_ = false;
    ws_client_.stop();

    if (worker_thread_.joinable()) {
        worker_thread_.join();
    }
}

bool KrakenBookClient::is_connected() const {
    return connected_;
}

bool KrakenBookClient::is_running() const {
    return running_;
}

void KrakenBookClient::set_update_callback(UpdateCallback callback) {
    std::lock_guard<std::mutex> lock(callback_mutex_);
    update_callback_ = callback;
}

void KrakenBookClient::set_connection_callback(ConnectionCallback callback) {
    std::lock_guard<std::mutex> lock(callback_mutex_);
    connection_callback_ = callback;
}

void KrakenBookClient::set_error_callback(ErrorCallback callback) {
    std::lock_guard<std::mutex> lock(callback_mutex_);
    error_callback_ = callback;
}

std::map<std::string, OrderBookStats> KrakenBookClient::get_stats() const {
    std::lock_guard<std::mutex> lock(stats_mutex_);
    return stats_;
}

KrakenBookClient::context_ptr KrakenBookClient::on_tls_init(websocketpp::connection_hdl) {
    context_ptr ctx = websocketpp::lib::make_shared<boost::asio::ssl::context>(
        boost::asio::ssl::context::tlsv12
    );

    try {
        ctx->set_options(
            boost::asio::ssl::context::default_workarounds |
            boost::asio::ssl::context::no_sslv2 |
            boost::asio::ssl::context::no_sslv3 |
            boost::asio::ssl::context::single_dh_use
        );
    } catch (std::exception& e) {
        notify_error(std::string("TLS init error: ") + e.what());
    }

    return ctx;
}

void KrakenBookClient::on_open(websocketpp::connection_hdl hdl) {
    connection_hdl_ = hdl;
    connected_ = true;
    notify_connection(true);

    // Send subscription
    std::string subscribe_msg = build_subscription();

    try {
        ws_client_.send(hdl, subscribe_msg, websocketpp::frame::opcode::text);
    } catch (const std::exception& e) {
        notify_error(std::string("Failed to send subscription: ") + e.what());
    }
}

void KrakenBookClient::on_close(websocketpp::connection_hdl) {
    connected_ = false;
    notify_connection(false);
}

void KrakenBookClient::on_fail(websocketpp::connection_hdl) {
    connected_ = false;
    notify_connection(false);
    notify_error("WebSocket connection failed");
}

void KrakenBookClient::on_message(websocketpp::connection_hdl, client::message_ptr msg) {
    const std::string& payload = msg->get_payload();
    process_book_message(payload);
}

void KrakenBookClient::run_client() {
    try {
        // Set handlers
        ws_client_.set_open_handler(std::bind(
            &KrakenBookClient::on_open, this, std::placeholders::_1
        ));
        ws_client_.set_close_handler(std::bind(
            &KrakenBookClient::on_close, this, std::placeholders::_1
        ));
        ws_client_.set_fail_handler(std::bind(
            &KrakenBookClient::on_fail, this, std::placeholders::_1
        ));
        ws_client_.set_message_handler(std::bind(
            &KrakenBookClient::on_message, this, std::placeholders::_1, std::placeholders::_2
        ));

        // Connect to Kraken WebSocket v2
        const std::string uri = "wss://ws.kraken.com/v2";
        websocketpp::lib::error_code ec;
        client::connection_ptr con = ws_client_.get_connection(uri, ec);

        if (ec) {
            notify_error(std::string("Connection init error: ") + ec.message());
            running_ = false;
            return;
        }

        ws_client_.connect(con);
        ws_client_.run();

    } catch (const std::exception& e) {
        notify_error(std::string("WebSocket error: ") + e.what());
        running_ = false;
    }
}

void KrakenBookClient::notify_connection(bool connected) {
    std::lock_guard<std::mutex> lock(callback_mutex_);
    if (connection_callback_) {
        connection_callback_(connected);
    }
}

void KrakenBookClient::notify_error(const std::string& error) {
    std::lock_guard<std::mutex> lock(callback_mutex_);
    if (error_callback_) {
        error_callback_(error);
    }
}

std::string KrakenBookClient::build_subscription() const {
    std::ostringstream oss;
    oss << R"({"method":"subscribe","params":{)";
    oss << R"("channel":"book",)";
    oss << R"("symbol":[)";

    for (size_t i = 0; i < symbols_.size(); ++i) {
        if (i > 0) oss << ",";
        oss << "\"" << symbols_[i] << "\"";
    }

    oss << R"(],"depth":)" << depth_ << ",";
    oss << R"("snapshot":true}})";

    return oss.str();
}

void KrakenBookClient::process_book_message(const std::string& payload) {
    try {
        simdjson::ondemand::parser parser;
        simdjson::padded_string padded(payload);
        simdjson::ondemand::document doc = parser.iterate(padded);

        // Handle subscription response
        if (auto method_result = doc["method"]; !method_result.error()) {
            std::string_view method = method_result.value();
            if (method == "subscribe") {
                if (auto success_result = doc["success"]; !success_result.error()) {
                    bool success = success_result.value();
                    if (success) {
                        std::cout << "[STATUS] Successfully subscribed to book channel" << std::endl;
                    } else {
                        std::cerr << "[ERROR] Book subscription failed" << std::endl;
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

            // Handle book messages
            if (channel == "book") {
                auto type_result = doc["type"];
                if (type_result.error()) return;

                std::string_view type_str = type_result.value();
                if (type_str != "snapshot" && type_str != "update") return;

                std::string timestamp = Utils::get_utc_timestamp();

                // Parse data array
                auto data_result = doc["data"];
                if (data_result.error()) return;

                simdjson::ondemand::array data_array = data_result.value();

                for (auto book_value : data_array) {
                    simdjson::ondemand::object book_obj = book_value.get_object();

                    OrderBookRecord record;
                    record.timestamp = timestamp;
                    record.type = std::string(type_str);

                    // Extract symbol
                    if (auto symbol = book_obj["symbol"]; !symbol.error()) {
                        std::string_view sv = symbol.value();
                        record.symbol = std::string(sv);
                    }

                    // Extract bids (v2 format: array of objects with "price" and "qty" fields)
                    if (auto bids = book_obj["bids"]; !bids.error()) {
                        simdjson::ondemand::array bids_array = bids.value();
                        for (auto bid_value : bids_array) {
                            simdjson::ondemand::object bid_obj = bid_value.get_object();

                            double price = 0.0;
                            double quantity = 0.0;

                            if (auto price_field = bid_obj["price"]; !price_field.error()) {
                                price = price_field.get_double();
                            }
                            if (auto qty_field = bid_obj["qty"]; !qty_field.error()) {
                                quantity = qty_field.get_double();
                            }

                            record.bids.emplace_back(price, quantity);
                        }
                    }

                    // Extract asks (v2 format: array of objects with "price" and "qty" fields)
                    if (auto asks = book_obj["asks"]; !asks.error()) {
                        simdjson::ondemand::array asks_array = asks.value();
                        for (auto ask_value : asks_array) {
                            simdjson::ondemand::object ask_obj = ask_value.get_object();

                            double price = 0.0;
                            double quantity = 0.0;

                            if (auto price_field = ask_obj["price"]; !price_field.error()) {
                                price = price_field.get_double();
                            }
                            if (auto qty_field = ask_obj["qty"]; !qty_field.error()) {
                                quantity = qty_field.get_double();
                            }

                            record.asks.emplace_back(price, quantity);
                        }
                    }

                    // Extract checksum
                    if (auto checksum = book_obj["checksum"]; !checksum.error()) {
                        record.checksum = static_cast<uint32_t>(checksum.get_uint64());
                    }

                    // Validate checksum if enabled
                    if (validate_checksums_ && !ChecksumValidator::validate(record)) {
                        std::cerr << "[WARNING] Checksum validation failed for "
                                  << record.symbol << std::endl;
                    }

                    // Update statistics
                    {
                        std::lock_guard<std::mutex> lock(stats_mutex_);
                        auto it = stats_.find(record.symbol);
                        if (it != stats_.end()) {
                            OrderBookDisplay::update_stats(it->second, record);
                        }
                    }

                    // Notify callback
                    {
                        std::lock_guard<std::mutex> lock(callback_mutex_);
                        if (update_callback_) {
                            update_callback_(record);
                        }
                    }
                }
            }
        }

    } catch (const simdjson::simdjson_error& e) {
        std::cerr << "[ERROR] simdjson parsing error: "
                  << simdjson::error_message(e.error()) << std::endl;
    }
}

} // namespace kraken

#endif // KRAKEN_BOOK_CLIENT_HPP
