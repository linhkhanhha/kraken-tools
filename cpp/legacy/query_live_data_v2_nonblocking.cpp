#include <iostream>
#include <string>
#include <vector>
#include <csignal>
#include <thread>
#include <atomic>
#include <websocketpp/config/asio_client.hpp>
#include <websocketpp/client.hpp>
#include <nlohmann/json.hpp>
#include "kraken_common.hpp"

using json = nlohmann::json;
using kraken::TickerRecord;
using kraken::Utils;

typedef websocketpp::client<websocketpp::config::asio_tls_client> client;
typedef websocketpp::lib::shared_ptr<websocketpp::lib::asio::ssl::context> context_ptr;

/**
 * Non-blocking WebSocket client for Kraken API v2
 *
 * Features:
 * - Runs ASIO event loop in separate thread
 * - Non-blocking API (poll-based or callback-based)
 * - Can integrate with existing event loops
 * - Thread-safe data access
 */
class KrakenWebSocketClient {
public:
    KrakenWebSocketClient() : running_(false), connected_(false) {}

    ~KrakenWebSocketClient() {
        stop();
    }

    /**
     * Start the WebSocket client (non-blocking)
     * Returns immediately, connection happens in background
     */
    bool start(const std::vector<std::string>& symbols) {
        if (running_) {
            std::cerr << "Client already running" << std::endl;
            return false;
        }

        symbols_ = symbols;
        running_ = true;

        // Start ASIO thread
        worker_thread_ = std::thread([this]() {
            this->run_client();
        });

        std::cout << "WebSocket client started in non-blocking mode" << std::endl;
        return true;
    }

    /**
     * Stop the WebSocket client
     */
    void stop() {
        if (!running_) return;

        running_ = false;
        connected_ = false;

        try {
            if (ws_client_.stopped()) {
                return;
            }

            // Stop ASIO event loop
            ws_client_.stop();

            // Wait for worker thread
            if (worker_thread_.joinable()) {
                worker_thread_.join();
            }

            std::cout << "WebSocket client stopped" << std::endl;
        } catch (const std::exception& e) {
            std::cerr << "Error stopping client: " << e.what() << std::endl;
        }
    }

    /**
     * Check if client is connected
     */
    bool is_connected() const {
        return connected_.load();
    }

    /**
     * Check if client is running
     */
    bool is_running() const {
        return running_.load();
    }

    /**
     * Get new ticker updates (non-blocking)
     * Returns all updates since last call, then clears buffer
     */
    std::vector<TickerRecord> get_updates() {
        std::lock_guard<std::mutex> lock(data_mutex_);
        std::vector<TickerRecord> updates = std::move(pending_updates_);
        pending_updates_.clear();
        return updates;
    }

    /**
     * Get all ticker history (non-blocking)
     */
    std::vector<TickerRecord> get_history() const {
        std::lock_guard<std::mutex> lock(data_mutex_);
        return ticker_history_;
    }

    /**
     * Get number of pending updates
     */
    size_t pending_count() const {
        std::lock_guard<std::mutex> lock(data_mutex_);
        return pending_updates_.size();
    }

    /**
     * Set callback for ticker updates (optional)
     */
    void set_update_callback(std::function<void(const TickerRecord&)> callback) {
        std::lock_guard<std::mutex> lock(callback_mutex_);
        update_callback_ = callback;
    }

    /**
     * Set callback for connection events (optional)
     */
    void set_connection_callback(std::function<void(bool)> callback) {
        std::lock_guard<std::mutex> lock(callback_mutex_);
        connection_callback_ = callback;
    }

    /**
     * Save current history to CSV
     */
    void save_to_csv(const std::string& filename) {
        std::lock_guard<std::mutex> lock(data_mutex_);
        Utils::save_to_csv(filename, ticker_history_);
    }

private:
    // WebSocket client
    client ws_client_;
    websocketpp::connection_hdl connection_hdl_;
    std::thread worker_thread_;

    // State
    std::atomic<bool> running_;
    std::atomic<bool> connected_;
    std::vector<std::string> symbols_;

    // Data storage (thread-safe)
    mutable std::mutex data_mutex_;
    std::vector<TickerRecord> ticker_history_;
    std::vector<TickerRecord> pending_updates_;

    // Callbacks (thread-safe)
    mutable std::mutex callback_mutex_;
    std::function<void(const TickerRecord&)> update_callback_;
    std::function<void(bool)> connection_callback_;

    // TLS/SSL context initialization
    context_ptr on_tls_init(websocketpp::connection_hdl) {
        context_ptr ctx = std::make_shared<boost::asio::ssl::context>(
            boost::asio::ssl::context::sslv23);

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
                    std::cout << "Successfully subscribed" << std::endl;
                } else {
                    std::cerr << "Subscription failed: " << data.dump() << std::endl;
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

                        // Store in history and pending updates
                        {
                            std::lock_guard<std::mutex> lock(data_mutex_);
                            ticker_history_.push_back(record);
                            pending_updates_.push_back(record);
                        }

                        // Call callback if set
                        {
                            std::lock_guard<std::mutex> lock(callback_mutex_);
                            if (update_callback_) {
                                update_callback_(record);
                            }
                        }

                        // Print to console
                        Utils::print_record(record);
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
        connection_hdl_ = hdl;
        connected_ = true;

        // Notify callback
        {
            std::lock_guard<std::mutex> lock(callback_mutex_);
            if (connection_callback_) {
                connection_callback_(true);
            }
        }

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
            std::cerr << "Send error: " << e.what() << std::endl;
        }
    }

    // Handle connection close
    void on_close(websocketpp::connection_hdl hdl) {
        std::cout << "WebSocket connection closed" << std::endl;
        connected_ = false;

        // Notify callback
        {
            std::lock_guard<std::mutex> lock(callback_mutex_);
            if (connection_callback_) {
                connection_callback_(false);
            }
        }
    }

    // Handle connection failure
    void on_fail(websocketpp::connection_hdl hdl) {
        std::cerr << "WebSocket connection failed" << std::endl;
        connected_ = false;

        // Notify callback
        {
            std::lock_guard<std::mutex> lock(callback_mutex_);
            if (connection_callback_) {
                connection_callback_(false);
            }
        }
    }

    // Main client loop (runs in worker thread)
    void run_client() {
        try {
            // Initialize WebSocket client
            ws_client_.init_asio();
            ws_client_.set_tls_init_handler([this](websocketpp::connection_hdl hdl) {
                return this->on_tls_init(hdl);
            });

            // Set handlers
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

            // Connect to Kraken WebSocket v2
            std::string uri = "wss://ws.kraken.com/v2";
            websocketpp::lib::error_code ec;
            client::connection_ptr con = ws_client_.get_connection(uri, ec);

            if (ec) {
                std::cerr << "Connection error: " << ec.message() << std::endl;
                running_ = false;
                return;
            }

            ws_client_.connect(con);

            std::cout << "Connecting to " << uri << "..." << std::endl;

            // Run the WebSocket client (this blocks until stopped)
            ws_client_.run();

        } catch (const websocketpp::exception& e) {
            std::cerr << "WebSocket++ exception: " << e.what() << std::endl;
        } catch (const std::exception& e) {
            std::cerr << "Exception: " << e.what() << std::endl;
        }

        running_ = false;
        connected_ = false;
    }
};

// Global client instance for signal handler
KrakenWebSocketClient* global_client = nullptr;

// Signal handler for Ctrl+C
void signal_handler(int signum) {
    std::cout << "\n\nStopping and saving data..." << std::endl;
    if (global_client) {
        global_client->save_to_csv("kraken_ticker_history_v2.csv");
        global_client->stop();
    }
    exit(0);
}

// Example 1: Poll-based usage (non-blocking, check periodically)
void example_poll_based() {
    std::cout << "\n=== Example 1: Poll-based (Non-blocking) ===" << std::endl;

    KrakenWebSocketClient client;

    // Start client (returns immediately)
    client.start({"BTC/USD", "ETH/USD", "SOL/USD"});

    // Main loop - can do other work here
    while (client.is_running()) {
        // Check for updates every 100ms
        std::this_thread::sleep_for(std::chrono::milliseconds(100));

        // Get any pending updates
        auto updates = client.get_updates();

        if (!updates.empty()) {
            std::cout << "Received " << updates.size() << " updates" << std::endl;
            // Process updates here...
        }

        // Can do other work in this loop
        // Your existing event loop can continue running
    }

    client.stop();
}

// Example 2: Callback-based usage (event-driven)
void example_callback_based() {
    std::cout << "\n=== Example 2: Callback-based (Event-driven) ===" << std::endl;

    KrakenWebSocketClient client;

    // Set callbacks
    client.set_update_callback([](const TickerRecord& record) {
        // This is called from worker thread!
        std::cout << "[Callback] " << record.pair << " = " << record.last << std::endl;
    });

    client.set_connection_callback([](bool connected) {
        std::cout << "[Callback] Connection " << (connected ? "opened" : "closed") << std::endl;
    });

    // Start client
    client.start({"BTC/USD", "ETH/USD"});

    // Wait for some time
    std::this_thread::sleep_for(std::chrono::seconds(10));

    client.stop();
}

int main() {
    // Register signal handler for Ctrl+C
    std::signal(SIGINT, signal_handler);

    std::cout << "Kraken WebSocket v2 - Non-blocking Version" << std::endl;
    std::cout << "===========================================" << std::endl;
    std::cout << std::endl;

    // Create non-blocking client
    KrakenWebSocketClient client;
    global_client = &client;

    // Set optional callback for real-time updates
    client.set_update_callback([](const TickerRecord& record) {
        // This callback is called from worker thread
        // Keep processing minimal here
        std::cout << "[Update] " << record.pair << " last=" << record.last
                  << " change=" << record.change_pct << "%" << std::endl;
    });

    client.set_connection_callback([](bool connected) {
        std::cout << "[Status] " << (connected ? "Connected" : "Disconnected") << std::endl;
    });

    // Start client (non-blocking - returns immediately)
    std::vector<std::string> symbols = {"BTC/USD", "ETH/USD", "SOL/USD"};
    if (!client.start(symbols)) {
        std::cerr << "Failed to start client" << std::endl;
        return 1;
    }

    std::cout << "Client started. Main thread is free to do other work..." << std::endl;
    std::cout << "Press Ctrl+C to stop and save data" << std::endl;
    std::cout << std::endl;

    // Main loop - can integrate with existing event loop
    while (client.is_running()) {
        // Option 1: Just wait
        std::this_thread::sleep_for(std::chrono::seconds(1));

        // Option 2: Poll for updates (alternative to callbacks)
        // auto updates = client.get_updates();
        // for (const auto& record : updates) {
        //     // Process updates
        // }

        // Option 3: Check status
        if (client.is_connected()) {
            std::cout << "[Status] Running... "
                      << client.pending_count() << " pending updates" << std::endl;
        }

        // Your existing application code can run here
        // The WebSocket runs independently in background thread
    }

    // Cleanup
    client.stop();
    global_client = nullptr;

    return 0;
}
