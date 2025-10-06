#include <iostream>
#include <string>
#include <vector>
#include <fstream>
#include <chrono>
#include <iomanip>
#include <sstream>
#include <csignal>
#include <websocketpp/config/asio_client.hpp>
#include <websocketpp/client.hpp>
#include <nlohmann/json.hpp>

using json = nlohmann::json;
typedef websocketpp::client<websocketpp::config::asio_tls_client> client;
typedef websocketpp::lib::shared_ptr<websocketpp::lib::asio::ssl::context> context_ptr;

// Store ticker history
struct TickerRecord {
    std::string timestamp;
    std::string pair;
    std::string type;
    double bid;
    double bid_qty;
    double ask;
    double ask_qty;
    double last;
    double volume;
    double vwap;
    double low;
    double high;
    double change;
    double change_pct;
};

std::vector<TickerRecord> ticker_history;
client ws_client;
websocketpp::connection_hdl global_hdl;

// Get current UTC timestamp
std::string get_utc_timestamp() {
    auto now = std::chrono::system_clock::now();
    auto time_t = std::chrono::system_clock::to_time_t(now);
    auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(now.time_since_epoch()) % 1000;

    std::tm tm = *std::gmtime(&time_t);
    std::ostringstream oss;
    oss << std::put_time(&tm, "%Y-%m-%d %H:%M:%S");
    oss << "." << std::setfill('0') << std::setw(3) << ms.count();
    return oss.str();
}

// Save data to CSV
void save_to_csv(const std::string& filename) {
    std::ofstream file(filename);

    // Write header
    file << "timestamp,pair,type,bid,bid_qty,ask,ask_qty,last,volume,vwap,low,high,change,change_pct\n";

    // Write data
    for (const auto& record : ticker_history) {
        file << record.timestamp << ","
             << record.pair << ","
             << record.type << ","
             << record.bid << ","
             << record.bid_qty << ","
             << record.ask << ","
             << record.ask_qty << ","
             << record.last << ","
             << record.volume << ","
             << record.vwap << ","
             << record.low << ","
             << record.high << ","
             << record.change << ","
             << record.change_pct << "\n";
    }

    file.close();
    std::cout << "\nSaved to " << filename << std::endl;
    std::cout << "Total records: " << ticker_history.size() << std::endl;
}

// Signal handler for Ctrl+C
void signal_handler(int signum) {
    std::cout << "\n\nStopping and saving data..." << std::endl;
    save_to_csv("kraken_ticker_history_v2.csv");

    // Close WebSocket connection
    try {
        ws_client.close(global_hdl, websocketpp::close::status::normal, "Client shutdown");
    } catch (const std::exception& e) {
        std::cerr << "Error closing connection: " << e.what() << std::endl;
    }

    exit(0);
}

// TLS/SSL context initialization
context_ptr on_tls_init(websocketpp::connection_hdl) {
    context_ptr ctx = std::make_shared<boost::asio::ssl::context>(boost::asio::ssl::context::sslv23);

    try {
        ctx->set_options(boost::asio::ssl::context::default_workarounds |
                        boost::asio::ssl::context::no_sslv2 |
                        boost::asio::ssl::context::no_sslv3 |
                        boost::asio::ssl::context::single_dh_use);
    } catch (std::exception& e) {
        std::cerr << "TLS init error: " << e.what() << std::endl;
    }

    return ctx;
}

// Handle incoming messages
void on_message(websocketpp::connection_hdl hdl, client::message_ptr msg) {
    try {
        json data = json::parse(msg->get_payload());

        // Handle subscription status
        if (data.contains("method") && data["method"] == "subscribe") {
            if (data.contains("success") && data["success"].get<bool>()) {
                std::cout << "Successfully subscribed: " << data.dump() << std::endl;
            } else {
                std::cout << "Subscription failed: " << data.dump() << std::endl;
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

            std::string timestamp = get_utc_timestamp();

            // Process ticker data array
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

                    ticker_history.push_back(record);

                    std::cout << timestamp << " | " << record.pair
                              << " | last: " << record.last
                              << " | change: " << std::fixed << std::setprecision(2)
                              << record.change_pct << "%" << std::endl;
                }
            }
        }

    } catch (const json::exception& e) {
        std::cerr << "JSON parsing error: " << e.what() << std::endl;
    } catch (const std::exception& e) {
        std::cerr << "Message handling error: " << e.what() << std::endl;
    }
}

// Handle connection open
void on_open(websocketpp::connection_hdl hdl) {
    std::cout << "WebSocket connection opened" << std::endl;

    // Send subscription message
    json subscribe_msg = {
        {"method", "subscribe"},
        {"params", {
            {"channel", "ticker"},
            {"symbol", {"BTC/USD", "ETH/USD", "SOL/USD"}},
            {"snapshot", true}
        }}
    };

    std::string msg_str = subscribe_msg.dump();
    std::cout << "Subscribing to: " << msg_str << std::endl;

    try {
        ws_client.send(hdl, msg_str, websocketpp::frame::opcode::text);
    } catch (const std::exception& e) {
        std::cerr << "Send error: " << e.what() << std::endl;
    }
}

// Handle connection close
void on_close(websocketpp::connection_hdl hdl) {
    std::cout << "WebSocket connection closed" << std::endl;
}

// Handle connection failure
void on_fail(websocketpp::connection_hdl hdl) {
    std::cerr << "WebSocket connection failed" << std::endl;
}

int main() {
    // Register signal handler for Ctrl+C
    std::signal(SIGINT, signal_handler);

    try {
        // Initialize WebSocket client
        ws_client.init_asio();
        ws_client.set_tls_init_handler(&on_tls_init);

        // Set handlers
        ws_client.set_open_handler(&on_open);
        ws_client.set_message_handler(&on_message);
        ws_client.set_close_handler(&on_close);
        ws_client.set_fail_handler(&on_fail);

        // Set logging
        ws_client.clear_access_channels(websocketpp::log::alevel::all);
        ws_client.set_access_channels(websocketpp::log::alevel::connect);
        ws_client.set_access_channels(websocketpp::log::alevel::disconnect);

        // Connect to Kraken WebSocket v2
        std::string uri = "wss://ws.kraken.com/v2";
        websocketpp::lib::error_code ec;
        client::connection_ptr con = ws_client.get_connection(uri, ec);

        if (ec) {
            std::cerr << "Connection error: " << ec.message() << std::endl;
            return 1;
        }

        global_hdl = con->get_handle();
        ws_client.connect(con);

        std::cout << "Connecting to " << uri << "..." << std::endl;

        // Run the WebSocket client (blocking)
        ws_client.run();

    } catch (const websocketpp::exception& e) {
        std::cerr << "WebSocket++ exception: " << e.what() << std::endl;
        return 1;
    } catch (const std::exception& e) {
        std::cerr << "Exception: " << e.what() << std::endl;
        return 1;
    }

    return 0;
}
