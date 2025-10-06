/**
 * Example 3b: Integration with Condition Variables (Responsive Version)
 *
 * This example demonstrates using condition variables instead of sleep
 * for immediate responsiveness to events. It shows:
 * - Who is the WAITER: Main event loop waiting for data
 * - Who is the SIGNALER: WebSocket callback notifying new data
 * - Immediate wakeup when data arrives (no sleep delay)
 * - Graceful shutdown with condition variable
 *
 * Compare with example_integration.cpp:
 * - Old: sleep_for(100ms) - up to 100ms delay to process data
 * - New: condition variable - immediate processing when data arrives
 *
 * Use this pattern when:
 * - Low latency is critical
 * - You need immediate response to events
 * - You want to minimize CPU usage while waiting
 */

#include <iostream>
#include <csignal>
#include <thread>
#include <chrono>
#include <atomic>
#include <queue>
#include <mutex>
#include <condition_variable>
#include "kraken_websocket_client_simdjson_v2.hpp"

using kraken::KrakenWebSocketClientSimdjsonV2;
using kraken::TickerRecord;

// Simulated existing system components (same as example_integration.cpp)
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

// Condition variable synchronization
std::mutex g_cv_mutex;
std::condition_variable g_cv;
bool g_new_data_available = false;  // Protected by g_cv_mutex

void signal_handler(int) {
    std::cout << "\n\nShutting down system..." << std::endl;
    g_running = false;

    // Wake up waiting thread for immediate shutdown
    g_cv.notify_all();
}

int main() {
    std::signal(SIGINT, signal_handler);

    std::cout << "==================================================" << std::endl;
    std::cout << "Example 3b: Condition Variable Integration" << std::endl;
    std::cout << "==================================================" << std::endl;
    std::cout << std::endl;

    // Create WebSocket client (using simdjson for performance)
    KrakenWebSocketClientSimdjsonV2 ws_client;
    g_ws_client = &ws_client;

    // =================================================================
    // SIGNALER: WebSocket callback sets flag and notifies
    // =================================================================
    ws_client.set_update_callback([&](const TickerRecord&) {
        // This callback runs in WebSocket thread - it's the SIGNALER

        // Step 1: Set the flag (under mutex protection)
        {
            std::lock_guard<std::mutex> lock(g_cv_mutex);
            g_new_data_available = true;
        }

        // Step 2: Notify waiting thread (main loop)
        g_cv.notify_one();

        // Result: Main loop wakes immediately instead of waiting for sleep!
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
    std::cout << "Using CONDITION VARIABLE for immediate responsiveness" << std::endl;
    std::cout << "Press Ctrl+C to shutdown." << std::endl;
    std::cout << std::endl;

    // =================================================================
    // WAITER: Main event loop waits for data with condition variable
    // =================================================================
    int cycle = 0;
    while (g_running && ws_client.is_running()) {
        cycle++;

        // Wait for new data OR periodic timeout OR shutdown signal
        {
            std::unique_lock<std::mutex> lock(g_cv_mutex);

            // Wait up to 1 second for new data, or until g_running becomes false
            // This is the WAITER - it sleeps efficiently until signaled
            bool woke_early = g_cv.wait_for(
                lock,
                std::chrono::seconds(1),
                [] { return g_new_data_available || !g_running; }
            );

            if (!g_running) {
                break;  // Shutdown requested
            }

            // Check if we have new data
            if (g_new_data_available) {
                g_new_data_available = false;  // Reset flag

                std::cout << "\n[WAKEUP] Woke immediately due to new data!" << std::endl;
            } else {
                // Timeout - periodic wakeup for housekeeping
                std::cout << "\n[TIMEOUT] Periodic wakeup (no new data)" << std::endl;
            }
        }  // Release lock

        // Process WebSocket updates
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

        // Process other system components
        g_trading_engine.process();
        g_risk_manager.monitor();
        g_dashboard.render();

        // Show status every 10 cycles
        if (cycle % 10 == 0) {
            std::cout << "[System] Running... cycle=" << cycle
                      << ", pending=" << ws_client.pending_count()
                      << std::endl;
        }
    }

    // Shutdown all components
    std::cout << "\nShutting down components..." << std::endl;

    ws_client.save_to_csv("kraken_ticker_data_cond.csv");
    ws_client.stop();

    std::cout << "System shutdown complete" << std::endl;

    return 0;
}

/*
 * KEY DIFFERENCES FROM example_integration.cpp:
 *
 * OLD APPROACH (sleep_for):
 * --------------------------
 * while (g_running) {
 *     // Check flag
 *     if (new_data_available.exchange(false)) {
 *         process_data();
 *     }
 *
 *     sleep_for(100ms);  // <-- Can delay up to 100ms!
 * }
 *
 * Problem: If data arrives 1ms after we start sleeping, we wait 99ms
 *
 *
 * NEW APPROACH (condition variable):
 * -----------------------------------
 * SIGNALER (WebSocket callback):
 *     {lock} g_new_data_available = true;
 *     notify_one();  // <-- Wakes waiter immediately!
 *
 * WAITER (Main loop):
 *     {lock} wait_for(1s, predicate);  // <-- Wakes instantly when notified
 *     if (g_new_data_available) {
 *         process_data();
 *     }
 *
 * Benefit: Data processed immediately, no sleep delay!
 *
 *
 * SHUTDOWN RESPONSIVENESS:
 * ------------------------
 * OLD: Ctrl+C pressed, but thread sleeps for up to 100ms before checking
 * NEW: Ctrl+C calls notify_all(), thread wakes instantly and exits
 *
 *
 * CPU EFFICIENCY:
 * ---------------
 * Both approaches use similar CPU when idle:
 * - sleep_for blocks the thread (no CPU usage)
 * - condition variable wait blocks the thread (no CPU usage)
 *
 * But condition variable is MORE RESPONSIVE while using SAME CPU!
 */
