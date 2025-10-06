/**
 * Example 3: Integration with Existing System
 *
 * This example shows how to integrate KrakenWebSocketClient
 * with an existing event-driven system. It demonstrates:
 * - Running WebSocket alongside other components
 * - Using hybrid polling + notification pattern
 * - Processing data in batches
 * - Coordinating multiple subsystems
 *
 * Use this pattern when:
 * - Integrating with existing codebase
 * - You have multiple concurrent components
 * - You need to coordinate data between systems
 */

#include <iostream>
#include <csignal>
#include <thread>
#include <chrono>
#include <atomic>
#include <queue>
#include <mutex>
#include "kraken_websocket_client_simdjson_v2.hpp"

using kraken::KrakenWebSocketClientSimdjsonV2;
using kraken::TickerRecord;

// Simulated existing system components
class TradingEngine {
public:
    void on_price_update(const TickerRecord& record) {
        std::cout << "[TradingEngine] Processing " << record.pair
                  << " at $" << record.last << std::endl;
        // Your trading logic here...
    }

    void process() {
        // Simulate trading engine work
    }
};

class RiskManager {
public:
    void check_exposure(const TickerRecord& record) {
        std::cout << "[RiskManager] Checking exposure for " << record.pair << std::endl;
        // Your risk management logic here...
    }

    void monitor() {
        // Simulate risk monitoring
    }
};

class Dashboard {
public:
    void update_display(const std::vector<TickerRecord>& records) {
        std::cout << "[Dashboard] Updating UI with " << records.size()
                  << " ticker updates" << std::endl;
        // Your UI update logic here...
    }

    void render() {
        // Simulate UI rendering
    }
};

// Global components
KrakenWebSocketClientSimdjsonV2* g_ws_client = nullptr;
TradingEngine g_trading_engine;
RiskManager g_risk_manager;
Dashboard g_dashboard;
std::atomic<bool> g_running{true};

void signal_handler(int) {
    std::cout << "\n\nShutting down system..." << std::endl;
    g_running = false;
}

int main() {
    std::signal(SIGINT, signal_handler);

    std::cout << "==================================================" << std::endl;
    std::cout << "Example 3: System Integration Pattern" << std::endl;
    std::cout << "==================================================" << std::endl;
    std::cout << std::endl;

    // Create WebSocket client (using simdjson for performance)
    KrakenWebSocketClientSimdjsonV2 ws_client;
    g_ws_client = &ws_client;

    // Flag for efficient notification (hybrid pattern)
    std::atomic<bool> new_data_available{false};

    // Set lightweight callback that just sets flag
    ws_client.set_update_callback([&](const TickerRecord&) {
        new_data_available = true;  // Just notify, don't process
    });

    ws_client.set_connection_callback([](bool connected) {
        std::cout << "[System] WebSocket " << (connected ? "connected" : "disconnected")
                  << std::endl;
    });

    // Start WebSocket client
    std::vector<std::string> symbols = {"BTC/USD", "ETH/USD", "SOL/USD"};
    if (!ws_client.start(symbols)) {
        std::cerr << "Failed to start WebSocket client" << std::endl;
        return 1;
    }

    std::cout << "System started. All components running..." << std::endl;
    std::cout << "Press Ctrl+C to shutdown." << std::endl;
    std::cout << std::endl;

    // Main system event loop
    int cycle = 0;
    while (g_running && ws_client.is_running()) {
        cycle++;

        // Check for WebSocket updates (efficient hybrid pattern)
        if (new_data_available.exchange(false)) {
            // Get all pending updates in one batch
            auto updates = ws_client.get_updates();

            if (!updates.empty()) {
                std::cout << "\n--- Cycle " << cycle << " ---" << std::endl;
                std::cout << "Processing " << updates.size() << " updates" << std::endl;

                // Distribute updates to all subsystems
                for (const auto& update : updates) {
                    g_trading_engine.on_price_update(update);
                    g_risk_manager.check_exposure(update);
                }

                // Batch update dashboard
                g_dashboard.update_display(updates);
            }
        }

        // Process other system components
        g_trading_engine.process();
        g_risk_manager.monitor();
        g_dashboard.render();

        // Your other application logic here...

        // Sleep to simulate work
        std::this_thread::sleep_for(std::chrono::milliseconds(100));

        // Show status every 10 cycles
        if (cycle % 10 == 0) {
            std::cout << "[System] Running... cycle=" << cycle
                      << ", pending=" << ws_client.pending_count()
                      << std::endl;
        }
    }

    // Shutdown all components
    std::cout << "\nShutting down components..." << std::endl;

    ws_client.save_to_csv("kraken_ticker_data.csv");
    ws_client.stop();

    std::cout << "System shutdown complete" << std::endl;

    return 0;
}
