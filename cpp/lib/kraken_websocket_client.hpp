#ifndef KRAKEN_WEBSOCKET_CLIENT_HPP
#define KRAKEN_WEBSOCKET_CLIENT_HPP

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
 * Non-blocking WebSocket client for Kraken API v2
 *
 * This client runs the WebSocket connection in a background thread,
 * leaving the main thread free for other work. It provides two ways
 * to access data:
 * 1. Polling: Call get_updates() periodically
 * 2. Callbacks: Set callbacks that fire when data arrives
 *
 * Thread Safety:
 * - All public methods are thread-safe
 * - Callbacks are called from the worker thread
 * - Data access is protected by mutexes
 *
 * Example usage:
 * @code
 *   KrakenWebSocketClient client;
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
class KrakenWebSocketClient {
public:
    // Type definitions
    using UpdateCallback = std::function<void(const TickerRecord&)>;
    using ConnectionCallback = std::function<void(bool connected)>;
    using ErrorCallback = std::function<void(const std::string& error)>;

    /**
     * Constructor
     */
    KrakenWebSocketClient();

    /**
     * Destructor - stops client if still running
     */
    ~KrakenWebSocketClient();

    // Disable copy (client manages threads)
    KrakenWebSocketClient(const KrakenWebSocketClient&) = delete;
    KrakenWebSocketClient& operator=(const KrakenWebSocketClient&) = delete;

    /**
     * Start the WebSocket client (non-blocking)
     *
     * This method returns immediately. The WebSocket connection
     * is established in a background thread.
     *
     * @param symbols List of symbol pairs to subscribe to (e.g., "BTC/USD")
     * @return true if start initiated successfully, false if already running
     */
    bool start(const std::vector<std::string>& symbols);

    /**
     * Stop the WebSocket client
     *
     * Closes the connection and waits for worker thread to finish.
     * Safe to call multiple times.
     */
    void stop();

    /**
     * Check if client is connected to WebSocket
     *
     * @return true if WebSocket connection is established
     */
    bool is_connected() const;

    /**
     * Check if client worker thread is running
     *
     * @return true if client is running (even if not yet connected)
     */
    bool is_running() const;

    /**
     * Get new ticker updates (non-blocking, thread-safe)
     *
     * Returns all updates received since the last call, then clears
     * the pending updates buffer.
     *
     * @return Vector of ticker records (may be empty)
     */
    std::vector<TickerRecord> get_updates();

    /**
     * Get all ticker history (non-blocking, thread-safe)
     *
     * Returns a copy of all ticker records received since start.
     *
     * @return Vector of all ticker records
     */
    std::vector<TickerRecord> get_history() const;

    /**
     * Get number of pending updates in buffer
     *
     * @return Number of updates waiting to be retrieved via get_updates()
     */
    size_t pending_count() const;

    /**
     * Set callback for ticker updates (optional, thread-safe)
     *
     * The callback is called from the worker thread immediately when
     * a ticker update arrives. Keep processing minimal in the callback.
     *
     * @param callback Function to call on each update (or nullptr to clear)
     */
    void set_update_callback(UpdateCallback callback);

    /**
     * Set callback for connection events (optional, thread-safe)
     *
     * Called when connection opens (true) or closes (false).
     *
     * @param callback Function to call on connection change (or nullptr to clear)
     */
    void set_connection_callback(ConnectionCallback callback);

    /**
     * Set callback for errors (optional, thread-safe)
     *
     * @param callback Function to call on errors (or nullptr to clear)
     */
    void set_error_callback(ErrorCallback callback);

    /**
     * Save current history to CSV (thread-safe)
     *
     * @param filename Output CSV filename
     */
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

#endif // KRAKEN_WEBSOCKET_CLIENT_HPP
