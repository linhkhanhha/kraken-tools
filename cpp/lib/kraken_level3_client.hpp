/**
 * Kraken WebSocket Client for Level 3 Order Book Data
 *
 * Subscribes to Kraken WebSocket v2 level3 channel with authentication
 * and processes individual order-level data.
 */

#ifndef KRAKEN_LEVEL3_CLIENT_HPP
#define KRAKEN_LEVEL3_CLIENT_HPP

#include <string>
#include <vector>
#include <thread>
#include <mutex>
#include <atomic>
#include <functional>
#include <sstream>
#include <iostream>
#include <map>
#include <fstream>
#include <cstdlib>
#include <websocketpp/config/asio_client.hpp>
#include <websocketpp/client.hpp>
#include <simdjson.h>
#include "level3_common.hpp"
#include "kraken_common.hpp"

namespace kraken {

/**
 * WebSocket client for Level 3 order book data
 * Requires authentication token
 */
class KrakenLevel3Client {
public:
    // Type definitions
    using UpdateCallback = std::function<void(const Level3Record&)>;
    using ConnectionCallback = std::function<void(bool connected)>;
    using ErrorCallback = std::function<void(const std::string& error)>;

    /**
     * Constructor
     * @param depth Order book depth (10, 100, or 1000)
     * @param token Authentication token (optional, can be set later)
     */
    KrakenLevel3Client(int depth = 10, const std::string& token = "");

    /**
     * Destructor
     */
    ~KrakenLevel3Client();

    // Disable copy
    KrakenLevel3Client(const KrakenLevel3Client&) = delete;
    KrakenLevel3Client& operator=(const KrakenLevel3Client&) = delete;

    /**
     * Set authentication token
     * Priority: explicit token > token file > environment variable
     */
    bool set_token(const std::string& token);
    bool set_token_from_file(const std::string& filepath);
    bool set_token_from_env();

    /**
     * Get authentication token (for verification)
     */
    bool has_token() const;

    /**
     * Start WebSocket connection and subscribe to symbols
     */
    bool start(const std::vector<std::string>& symbols);

    /**
     * Stop WebSocket connection
     */
    void stop();

    /**
     * Check if connected
     */
    bool is_connected() const;

    /**
     * Check if running
     */
    bool is_running() const;

    /**
     * Set callbacks
     */
    void set_update_callback(UpdateCallback callback);
    void set_connection_callback(ConnectionCallback callback);
    void set_error_callback(ErrorCallback callback);

    /**
     * Get statistics per symbol
     */
    std::map<std::string, Level3Stats> get_stats() const;

private:
    // WebSocket types
    typedef websocketpp::client<websocketpp::config::asio_tls_client> client;
    typedef websocketpp::lib::shared_ptr<websocketpp::lib::asio::ssl::context> context_ptr;

    // Configuration
    int depth_;
    std::string token_;

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
    std::map<std::string, Level3Stats> stats_;

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
    void process_level3_message(const std::string& payload);
    std::string build_subscription() const;
    std::string read_token_file(const std::string& filepath);
};

} // namespace kraken

#endif // KRAKEN_LEVEL3_CLIENT_HPP
