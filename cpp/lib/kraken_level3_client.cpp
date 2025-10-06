/**
 * Kraken Level 3 WebSocket Client - Implementation
 */

#include "kraken_level3_client.hpp"

namespace kraken {

// ============================================================================
// KrakenLevel3Client Implementation
// ============================================================================

KrakenLevel3Client::KrakenLevel3Client(int depth, const std::string& token)
    : depth_(depth), token_(token), running_(false), connected_(false) {

    // Initialize WebSocket client
    ws_client_.clear_access_channels(websocketpp::log::alevel::all);
    ws_client_.clear_error_channels(websocketpp::log::elevel::all);
    ws_client_.init_asio();
    ws_client_.set_tls_init_handler(std::bind(
        &KrakenLevel3Client::on_tls_init, this, std::placeholders::_1
    ));
}

KrakenLevel3Client::~KrakenLevel3Client() {
    stop();
}

bool KrakenLevel3Client::set_token(const std::string& token) {
    if (token.empty()) {
        return false;
    }
    token_ = token;
    return true;
}

bool KrakenLevel3Client::set_token_from_file(const std::string& filepath) {
    std::string token = read_token_file(filepath);
    if (token.empty()) {
        return false;
    }
    token_ = token;
    return true;
}

bool KrakenLevel3Client::set_token_from_env() {
    const char* env_token = std::getenv("KRAKEN_WS_TOKEN");
    if (env_token == nullptr || strlen(env_token) == 0) {
        return false;
    }
    token_ = std::string(env_token);
    return true;
}

std::string KrakenLevel3Client::read_token_file(const std::string& filepath) {
    std::ifstream file(filepath);
    if (!file.is_open()) {
        std::cerr << "Error: Cannot open token file: " << filepath << std::endl;
        return "";
    }

    std::string token;
    std::getline(file, token);
    file.close();

    // Trim whitespace
    token.erase(0, token.find_first_not_of(" \t\n\r"));
    token.erase(token.find_last_not_of(" \t\n\r") + 1);

    return token;
}

bool KrakenLevel3Client::has_token() const {
    return !token_.empty();
}

bool KrakenLevel3Client::start(const std::vector<std::string>& symbols) {
    if (running_) {
        return false;
    }

    if (!has_token()) {
        notify_error("No authentication token provided. Set via --token, --token-file, or KRAKEN_WS_TOKEN environment variable.");
        return false;
    }

    symbols_ = symbols;
    running_ = true;

    // Initialize statistics
    {
        std::lock_guard<std::mutex> lock(stats_mutex_);
        stats_.clear();
        for (const auto& symbol : symbols_) {
            stats_[symbol] = Level3Stats();
        }
    }

    // Start worker thread
    worker_thread_ = std::thread(&KrakenLevel3Client::run_client, this);

    return true;
}

void KrakenLevel3Client::stop() {
    if (!running_) {
        return;
    }

    running_ = false;
    ws_client_.stop();

    if (worker_thread_.joinable()) {
        worker_thread_.join();
    }
}

bool KrakenLevel3Client::is_connected() const {
    return connected_;
}

bool KrakenLevel3Client::is_running() const {
    return running_;
}

void KrakenLevel3Client::set_update_callback(UpdateCallback callback) {
    std::lock_guard<std::mutex> lock(callback_mutex_);
    update_callback_ = callback;
}

void KrakenLevel3Client::set_connection_callback(ConnectionCallback callback) {
    std::lock_guard<std::mutex> lock(callback_mutex_);
    connection_callback_ = callback;
}

void KrakenLevel3Client::set_error_callback(ErrorCallback callback) {
    std::lock_guard<std::mutex> lock(callback_mutex_);
    error_callback_ = callback;
}

std::map<std::string, Level3Stats> KrakenLevel3Client::get_stats() const {
    std::lock_guard<std::mutex> lock(stats_mutex_);
    return stats_;
}

KrakenLevel3Client::context_ptr KrakenLevel3Client::on_tls_init(websocketpp::connection_hdl) {
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

void KrakenLevel3Client::on_open(websocketpp::connection_hdl hdl) {
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

void KrakenLevel3Client::on_close(websocketpp::connection_hdl) {
    connected_ = false;
    notify_connection(false);
}

void KrakenLevel3Client::on_fail(websocketpp::connection_hdl) {
    connected_ = false;
    notify_connection(false);
    notify_error("WebSocket connection failed");
}

void KrakenLevel3Client::on_message(websocketpp::connection_hdl, client::message_ptr msg) {
    const std::string& payload = msg->get_payload();
    process_level3_message(payload);
}

void KrakenLevel3Client::run_client() {
    try {
        // Set handlers
        ws_client_.set_open_handler(std::bind(
            &KrakenLevel3Client::on_open, this, std::placeholders::_1
        ));
        ws_client_.set_close_handler(std::bind(
            &KrakenLevel3Client::on_close, this, std::placeholders::_1
        ));
        ws_client_.set_fail_handler(std::bind(
            &KrakenLevel3Client::on_fail, this, std::placeholders::_1
        ));
        ws_client_.set_message_handler(std::bind(
            &KrakenLevel3Client::on_message, this, std::placeholders::_1, std::placeholders::_2
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

void KrakenLevel3Client::notify_connection(bool connected) {
    std::lock_guard<std::mutex> lock(callback_mutex_);
    if (connection_callback_) {
        connection_callback_(connected);
    }
}

void KrakenLevel3Client::notify_error(const std::string& error) {
    std::lock_guard<std::mutex> lock(callback_mutex_);
    if (error_callback_) {
        error_callback_(error);
    }
}

std::string KrakenLevel3Client::build_subscription() const {
    std::ostringstream oss;
    oss << R"({"method":"subscribe","params":{)";
    oss << R"("channel":"level3",)";
    oss << R"("symbol":[)";

    for (size_t i = 0; i < symbols_.size(); ++i) {
        if (i > 0) oss << ",";
        oss << "\"" << symbols_[i] << "\"";
    }

    oss << R"(],"depth":)" << depth_ << ",";
    oss << R"("snapshot":true,)";
    oss << R"("token":")" << token_ << R"("})})";

    return oss.str();
}

void KrakenLevel3Client::process_level3_message(const std::string& payload) {
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
                        std::cout << "[STATUS] Successfully subscribed to level3 channel" << std::endl;
                    } else {
                        // Get error message if available
                        std::string error_msg = "Level3 subscription failed";
                        if (auto error_field = doc["error"]; !error_field.error()) {
                            std::string_view err_sv = error_field.value();
                            error_msg = std::string(err_sv);
                        }
                        std::cerr << "[ERROR] " << error_msg << std::endl;
                        notify_error(error_msg);
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

            // Handle level3 messages
            if (channel == "level3") {
                auto type_result = doc["type"];
                if (type_result.error()) return;

                std::string_view type_str = type_result.value();
                if (type_str != "snapshot" && type_str != "update") return;

                std::string timestamp = Utils::get_utc_timestamp();

                // Parse data array
                auto data_result = doc["data"];
                if (data_result.error()) return;

                simdjson::ondemand::array data_array = data_result.value();

                for (auto level3_value : data_array) {
                    simdjson::ondemand::object level3_obj = level3_value.get_object();

                    Level3Record record;
                    record.timestamp = timestamp;
                    record.type = std::string(type_str);

                    // Extract symbol
                    if (auto symbol = level3_obj["symbol"]; !symbol.error()) {
                        std::string_view sv = symbol.value();
                        record.symbol = std::string(sv);
                    }

                    // Extract bids (Level 3: array of orders)
                    if (auto bids = level3_obj["bids"]; !bids.error()) {
                        simdjson::ondemand::array bids_array = bids.value();
                        for (auto bid_value : bids_array) {
                            simdjson::ondemand::object bid_obj = bid_value.get_object();

                            Level3Order order;

                            // Event (for updates only)
                            if (auto event_field = bid_obj["event"]; !event_field.error()) {
                                std::string_view event_sv = event_field.value();
                                order.event = std::string(event_sv);
                            }

                            // Order ID
                            if (auto order_id = bid_obj["order_id"]; !order_id.error()) {
                                std::string_view id_sv = order_id.value();
                                order.order_id = std::string(id_sv);
                            }

                            // Limit price
                            if (auto limit_price = bid_obj["limit_price"]; !limit_price.error()) {
                                order.limit_price = limit_price.get_double();
                            }

                            // Order quantity
                            if (auto order_qty = bid_obj["order_qty"]; !order_qty.error()) {
                                order.order_qty = order_qty.get_double();
                            }

                            // Timestamp
                            if (auto ts = bid_obj["timestamp"]; !ts.error()) {
                                std::string_view ts_sv = ts.value();
                                order.timestamp = std::string(ts_sv);
                            }

                            record.bids.push_back(order);
                        }
                    }

                    // Extract asks (same structure as bids)
                    if (auto asks = level3_obj["asks"]; !asks.error()) {
                        simdjson::ondemand::array asks_array = asks.value();
                        for (auto ask_value : asks_array) {
                            simdjson::ondemand::object ask_obj = ask_value.get_object();

                            Level3Order order;

                            if (auto event_field = ask_obj["event"]; !event_field.error()) {
                                std::string_view event_sv = event_field.value();
                                order.event = std::string(event_sv);
                            }

                            if (auto order_id = ask_obj["order_id"]; !order_id.error()) {
                                std::string_view id_sv = order_id.value();
                                order.order_id = std::string(id_sv);
                            }

                            if (auto limit_price = ask_obj["limit_price"]; !limit_price.error()) {
                                order.limit_price = limit_price.get_double();
                            }

                            if (auto order_qty = ask_obj["order_qty"]; !order_qty.error()) {
                                order.order_qty = order_qty.get_double();
                            }

                            if (auto ts = ask_obj["timestamp"]; !ts.error()) {
                                std::string_view ts_sv = ts.value();
                                order.timestamp = std::string(ts_sv);
                            }

                            record.asks.push_back(order);
                        }
                    }

                    // Extract checksum
                    if (auto checksum = level3_obj["checksum"]; !checksum.error()) {
                        record.checksum = static_cast<uint32_t>(checksum.get_uint64());
                    }

                    // Update statistics
                    {
                        std::lock_guard<std::mutex> lock(stats_mutex_);
                        auto it = stats_.find(record.symbol);
                        if (it != stats_.end()) {
                            Level3Display::update_stats(it->second, record);
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
