/**
 * Example demonstrating template-based WebSocket clients
 *
 * This shows how the template refactoring eliminates code duplication.
 * Both nlohmann and simdjson versions use the same base implementation.
 */

#include "kraken_websocket_client_v2.hpp"
#include "kraken_websocket_client_simdjson_v2.hpp"
#include <iostream>
#include <thread>
#include <chrono>

using namespace kraken;

int main() {
    std::cout << "==================================================\n";
    std::cout << "Template-Based WebSocket Client Demo\n";
    std::cout << "==================================================\n\n";

    // Choose which implementation to use (same API for both!)
    std::cout << "Select JSON parser:\n";
    std::cout << "1. nlohmann/json (easier to debug)\n";
    std::cout << "2. simdjson (2-5x faster)\n";
    std::cout << "Choice (1 or 2): ";

    int choice;
    std::cin >> choice;

    if (choice == 1) {
        std::cout << "\nUsing nlohmann/json parser...\n\n";

        KrakenWebSocketClientV2 client;

        client.set_update_callback([](const TickerRecord& record) {
            std::cout << record.pair << ": " << record.last
                      << " (" << record.change_pct << "%)\n";
        });

        client.start({"BTC/USD", "ETH/USD"});

        std::cout << "Running for 10 seconds...\n";
        std::this_thread::sleep_for(std::chrono::seconds(10));

        client.stop();

        std::cout << "\nReceived " << client.get_history().size() << " updates\n";

    } else if (choice == 2) {
        std::cout << "\nUsing simdjson parser...\n\n";

        KrakenWebSocketClientSimdjsonV2 client;

        client.set_update_callback([](const TickerRecord& record) {
            std::cout << record.pair << ": " << record.last
                      << " (" << record.change_pct << "%)\n";
        });

        client.start({"BTC/USD", "ETH/USD"});

        std::cout << "Running for 10 seconds...\n";
        std::this_thread::sleep_for(std::chrono::seconds(10));

        client.stop();

        std::cout << "\nReceived " << client.get_history().size() << " updates\n";

    } else {
        std::cout << "Invalid choice\n";
        return 1;
    }

    std::cout << "\n==================================================\n";
    std::cout << "Template version demonstrates:\n";
    std::cout << "- Zero code duplication\n";
    std::cout << "- Easy to add new JSON parsers\n";
    std::cout << "- Same API for all implementations\n";
    std::cout << "- Type-safe at compile time\n";
    std::cout << "==================================================\n";

    return 0;
}
