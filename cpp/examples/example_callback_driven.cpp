/**
 * Example 2: Callback-driven (Event-driven)
 *
 * This example shows how to use callbacks for immediate notification:
 * - Set update callback for real-time notifications
 * - Set connection callback to monitor connectivity
 * - Set error callback for error handling
 *
 * Use this pattern when:
 * - You need immediate response to updates
 * - You have an event-driven architecture
 * - Low latency is important
 *
 * NOTE: Callbacks run in the worker thread!
 */

#include <iostream>
#include <csignal>
#include <thread>
#include <chrono>
#include <atomic>
#include "kraken_websocket_client_simdjson_v2.hpp"

using kraken::KrakenWebSocketClientSimdjsonV2;
using kraken::TickerRecord;

// Global client for signal handler
KrakenWebSocketClientSimdjsonV2* g_client = nullptr;
std::atomic<int> g_update_count{0};

void signal_handler(int) {
    std::cout << "\n\nShutting down..." << std::endl;
    if (g_client) {
        g_client->save_to_csv("kraken_ticker_data.csv");
        g_client->stop();
    }
    exit(0);
}

int main() {
    std::signal(SIGINT, signal_handler);

    std::cout << "==================================================" << std::endl;
    std::cout << "Example 2: Callback-driven Pattern" << std::endl;
    std::cout << "==================================================" << std::endl;
    std::cout << std::endl;

    // Create client (using simdjson for performance)
    KrakenWebSocketClientSimdjsonV2 client;
    g_client = &client;

    // Set update callback - called immediately when data arrives
    // WARNING: This runs in worker thread!
    client.set_update_callback([](const TickerRecord& record) {
        // Keep processing minimal here
        std::cout << "[Update] " << record.pair
                  << " = $" << record.last
                  << " (" << (record.change_pct > 0 ? "+" : "")
                  << record.change_pct << "%)"
                  << std::endl;

        g_update_count++;
    });

    // Set connection callback
    client.set_connection_callback([](bool connected) {
        std::cout << "[Connection] " << (connected ? "CONNECTED" : "DISCONNECTED")
                  << std::endl;
    });

    // Set error callback
    client.set_error_callback([](const std::string& error) {
        std::cerr << "[Error] " << error << std::endl;
    });

    // Define symbols
    std::vector<std::string> symbols = {"BTC/USD", "ETH/USD", "SOL/USD"};

    // Start client
    if (!client.start(symbols)) {
        std::cerr << "Failed to start client" << std::endl;
        return 1;
    }

    std::cout << "Client started. Callbacks will fire automatically." << std::endl;
    std::cout << "Press Ctrl+C to stop." << std::endl;
    std::cout << std::endl;

    // Main thread is now free to do other work
    // Callbacks will fire automatically when data arrives
    while (client.is_running()) {
        // Just sleep - callbacks handle everything
        std::this_thread::sleep_for(std::chrono::seconds(1));

        // Show status
        std::cout << "[Status] Running... "
                  << g_update_count << " updates received, "
                  << client.pending_count() << " pending"
                  << std::endl;

        // Your application can do other work here
        // The WebSocket runs independently
    }

    // Cleanup
    std::cout << "Saving data..." << std::endl;
    client.save_to_csv("kraken_ticker_data.csv");
    client.stop();

    std::cout << "Done! Processed " << g_update_count << " updates" << std::endl;

    return 0;
}
