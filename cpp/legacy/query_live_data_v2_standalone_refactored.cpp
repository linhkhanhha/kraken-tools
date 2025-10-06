#include <iostream>
#include <vector>
#include <csignal>
#include <thread>
#include <chrono>
#include "kraken_common.hpp"

using kraken::TickerRecord;
using kraken::Utils;

// Store ticker history
std::vector<TickerRecord> ticker_history;
bool running = true;

// Signal handler for Ctrl+C
void signal_handler(int signum) {
    std::cout << "\n\nReceived signal, stopping..." << std::endl;
    running = false;
    Utils::save_to_csv("kraken_ticker_history_v2.csv", ticker_history);
    exit(0);
}

int main() {
    std::signal(SIGINT, signal_handler);

    std::cout << "==================================================" << std::endl;
    std::cout << "Kraken WebSocket v2 - Standalone Version (Demo)" << std::endl;
    std::cout << "==================================================" << std::endl;
    std::cout << std::endl;
    std::cout << "Note: This is a simplified standalone version." << std::endl;
    std::cout << "It demonstrates the data structure and CSV export" << std::endl;
    std::cout << "but does NOT implement WebSocket connectivity." << std::endl;
    std::cout << std::endl;
    std::cout << "For full WebSocket support, use query_live_data_v2" << std::endl;
    std::cout << std::endl;

    std::cout << "To build the full version with dependencies:" << std::endl;
    std::cout << "  1. Install system packages:" << std::endl;
    std::cout << "     sudo yum install boost-devel openssl-devel" << std::endl;
    std::cout << std::endl;
    std::cout << "  2. Clone websocketpp:" << std::endl;
    std::cout << "     git clone https://github.com/zaphoyd/websocketpp.git" << std::endl;
    std::cout << std::endl;
    std::cout << "  3. Download nlohmann/json:" << std::endl;
    std::cout << "     mkdir -p include/nlohmann" << std::endl;
    std::cout << "     wget -O include/nlohmann/json.hpp \\" << std::endl;
    std::cout << "       https://github.com/nlohmann/json/releases/download/v3.11.3/json.hpp" << std::endl;
    std::cout << std::endl;
    std::cout << "  4. Build with CMake:" << std::endl;
    std::cout << "     cmake -B build -S ." << std::endl;
    std::cout << "     cmake --build build" << std::endl;
    std::cout << "     ./build/query_live_data_v2" << std::endl;
    std::cout << std::endl;

    std::cout << "==================================================" << std::endl;
    std::cout << "Demo: Ticker Data Structure" << std::endl;
    std::cout << "==================================================" << std::endl;
    std::cout << std::endl;

    // Create sample ticker records to demonstrate the structure
    TickerRecord sample1;
    sample1.timestamp = Utils::get_utc_timestamp();
    sample1.pair = "BTC/USD";
    sample1.type = "snapshot";
    sample1.bid = 64250.5;
    sample1.bid_qty = 1.23456;
    sample1.ask = 64251.0;
    sample1.ask_qty = 2.34567;
    sample1.last = 64250.7;
    sample1.volume = 1234.56;
    sample1.vwap = 64200.3;
    sample1.low = 63500.0;
    sample1.high = 65000.0;
    sample1.change = 750.5;
    sample1.change_pct = 1.18;

    TickerRecord sample2;
    sample2.timestamp = Utils::get_utc_timestamp();
    sample2.pair = "ETH/USD";
    sample2.type = "update";
    sample2.bid = 3425.8;
    sample2.bid_qty = 5.67890;
    sample2.ask = 3426.2;
    sample2.ask_qty = 4.56789;
    sample2.last = 3426.0;
    sample2.volume = 5678.90;
    sample2.vwap = 3420.5;
    sample2.low = 3400.0;
    sample2.high = 3450.0;
    sample2.change = 25.5;
    sample2.change_pct = 0.75;

    ticker_history.push_back(sample1);
    ticker_history.push_back(sample2);

    std::cout << "Sample ticker data:" << std::endl;
    std::cout << std::endl;
    Utils::print_csv_header();
    Utils::print_record(sample1);
    Utils::print_record(sample2);
    std::cout << std::endl;

    std::cout << "Saving sample data to CSV..." << std::endl;
    Utils::save_to_csv("kraken_ticker_demo.csv", ticker_history);

    std::cout << std::endl;
    std::cout << "Press Ctrl+C to exit" << std::endl;

    // Keep running until Ctrl+C
    while (running) {
        std::this_thread::sleep_for(std::chrono::seconds(1));
    }

    return 0;
}
