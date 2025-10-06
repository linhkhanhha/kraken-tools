#ifndef KRAKEN_WEBSOCKET_CLIENT_SIMDJSON_HPP
#define KRAKEN_WEBSOCKET_CLIENT_SIMDJSON_HPP

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
 * Non-blocking WebSocket client for Kraken API v2 (simdjson version)
 *
 * This version uses simdjson for JSON parsing, providing:
 * - 2-5x faster parsing than nlohmann/json
 * - Zero-copy parsing (no memory allocation)
 * - Lower CPU usage
 * - Better scalability (handles 1000+ msg/sec)
 *
 * API is identical to KrakenWebSocketClient (nlohmann version)
 * for easy comparison and migration.
 *
 * Example usage:
 * @code
 *   KrakenWebSocketClientSimdjson client;
 *   client.start({"BTC/USD", "ETH/USD"});
 *
 *   while (client.is_running()) {
 *       auto updates = client.get_updates();
 *       // Process updates...
 *   }
 *
 *   client.stop();
 * @endcode
 */
class KrakenWebSocketClientSimdjson {
public:
    // Type definitions (identical to nlohmann version)
    using UpdateCallback = std::function<void(const TickerRecord&)>;
    using ConnectionCallback = std::function<void(bool connected)>;
    using ErrorCallback = std::function<void(const std::string& error)>;

    /**
     * Constructor
     */
    KrakenWebSocketClientSimdjson();

    /**
     * Destructor - stops client if still running
     */
    ~KrakenWebSocketClientSimdjson();

    // Disable copy
    KrakenWebSocketClientSimdjson(const KrakenWebSocketClientSimdjson&) = delete;
    KrakenWebSocketClientSimdjson& operator=(const KrakenWebSocketClientSimdjson&) = delete;

    // Public API - identical to nlohmann version

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

private:
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
};

} // namespace kraken

#endif // KRAKEN_WEBSOCKET_CLIENT_SIMDJSON_HPP
