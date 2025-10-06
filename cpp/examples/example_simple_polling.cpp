/**
 * Example 1: Simple Polling
 *
 * This example shows the simplest way to use KrakenWebSocketClient:
 * - Start the client
 * - Poll for updates periodically
 * - Process updates in main thread
 *
 * Use this pattern when:
 * - You have a simple main loop
 * - You want to control when to process updates
 * - You prefer polling over callbacks
 */

#include <iostream>
#include <csignal>
#include <thread>
#include <chrono>
#include "kraken_websocket_client_simdjson_v2.hpp"

using kraken::KrakenWebSocketClientSimdjsonV2;
using kraken::TickerRecord;

// Global client for signal handler
KrakenWebSocketClientSimdjsonV2* g_client = nullptr;

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
    std::cout << "Example 1: Simple Polling Pattern" << std::endl;
    std::cout << "==================================================" << std::endl;
    std::cout << std::endl;

    // Create client (using simdjson for performance)
    KrakenWebSocketClientSimdjsonV2 client;
    g_client = &client;

    // Define symbols to monitor
    std::vector<std::string> symbols = {"BTC/USD", "ETH/USD", "SOL/USD"};

    // Start client (non-blocking - returns immediately)
    if (!client.start(symbols)) {
        std::cerr << "Failed to start client" << std::endl;
        return 1;
    }

    std::cout << "Client started. Press Ctrl+C to stop." << std::endl;
    std::cout << std::endl;

    // Simple polling loop
    int update_count = 0;
    while (client.is_running()) {
        // Wait before checking for updates
        std::this_thread::sleep_for(std::chrono::milliseconds(500));

        // Get all pending updates (non-blocking)
        auto updates = client.get_updates();

        if (!updates.empty()) {
            std::cout << "Received " << updates.size() << " updates:" << std::endl;

            for (const auto& record : updates) {
                std::cout << "  " << record.pair
                          << " = $" << record.last
                          << " (" << (record.change_pct > 0 ? "+" : "")
                          << record.change_pct << "%)"
                          << std::endl;

                update_count++;
            }

            std::cout << "Total updates so far: " << update_count << std::endl;
            std::cout << std::endl;
        }

        // You can do other work here...
        // This is your main application loop
    }

    // Cleanup
    std::cout << "Saving data..." << std::endl;
    client.save_to_csv("kraken_ticker_data.csv");
    client.stop();

    std::cout << "Done! Processed " << update_count << " updates" << std::endl;

    return 0;
}
