/**
 * Kraken Live Data Retriever - Level 1 (Ticker Data)
 *
 * A production-ready tool for retrieving real-time ticker data from Kraken.
 * Supports flexible pair input via command-line arguments:
 * - Direct pair list: -p "BTC/USD,ETH/USD,SOL/USD"
 * - CSV file: -p /path/to/file.csv:column_name:limit
 *
 * Usage:
 *   ./retrieve_kraken_live_data_level1 -p "BTC/USD,ETH/USD,SOL/USD"
 *   ./retrieve_kraken_live_data_level1 -p /export1/rocky/dev/kraken/kraken_usd_volume.csv:pair:10
 *   ./retrieve_kraken_live_data_level1 -p /export1/rocky/dev/kraken/kraken_usd_volume.csv:pair
 *   ./retrieve_kraken_live_data_level1 -p pairs.csv:symbol
 *
 * Output:
 *   Saves ticker data to kraken_ticker_live_level1.csv
 */

#include <iostream>
#include <iomanip>
#include <csignal>
#include <chrono>
#include <atomic>
#include <vector>
#include <string>
#include <mutex>
#include <condition_variable>
#include "kraken_websocket_client_simdjson_v2.hpp"
#include "cli_utils.hpp"

using kraken::KrakenWebSocketClientSimdjsonV2;
using kraken::TickerRecord;

// Global state
KrakenWebSocketClientSimdjsonV2* g_ws_client = nullptr;
std::atomic<bool> g_running{true};
std::mutex g_cv_mutex;
std::condition_variable g_cv;
bool g_new_data_available = false;

void signal_handler(int) {
    std::cout << "\n\nShutting down..." << std::endl;
    g_running = false;
    g_cv.notify_all();
}

void print_usage_examples() {
    std::cout << std::endl;
    std::cout << "Examples:" << std::endl;
    std::cout << "  1. Direct list (comma-separated):" << std::endl;
    std::cout << "     -p \"BTC/USD,ETH/USD,SOL/USD\"" << std::endl;
    std::cout << std::endl;
    std::cout << "  2. Text file (one pair per line, no header):" << std::endl;
    std::cout << "     -p kraken_tickers.txt          # All lines" << std::endl;
    std::cout << "     -p kraken_tickers.txt:10       # First 10 lines" << std::endl;
    std::cout << std::endl;
    std::cout << "  3. CSV file (with column name):" << std::endl;
    std::cout << "     -p kraken_usd_volume.csv:pair       # All rows" << std::endl;
    std::cout << "     -p kraken_usd_volume.csv:pair:10    # First 10 rows" << std::endl;
    std::cout << std::endl;
}

int main(int argc, char* argv[]) {
    // Setup argument parser
    cli::ArgumentParser parser(argv[0], "Retrieve real-time Level 1 ticker data from Kraken");

    parser.add_argument({
        "-p", "--pairs",
        "Pairs specification (direct list or CSV file)",
        true,  // required
        true,  // has value
        "",
        "SPEC"
    });

    parser.add_argument({
        "-o", "--output",
        "Output CSV filename",
        false,  // optional
        true,   // has value
        "kraken_ticker_live_level1.csv",
        "FILE"
    });

    parser.add_argument({
        "-f", "--flush-interval",
        "Flush interval in seconds (0 to disable time-based flush)",
        false,  // optional
        true,   // has value
        "30",
        "SECONDS"
    });

    parser.add_argument({
        "-m", "--memory-threshold",
        "Memory threshold in bytes (0 to disable memory-based flush)",
        false,  // optional
        true,   // has value
        "10485760",  // 10MB default
        "BYTES"
    });

    parser.add_argument({
        "", "--hourly",
        "Enable hourly file segmentation (output.20251112_10.csv)",
        false,  // optional
        false,  // no value (flag)
        "",
        ""
    });

    parser.add_argument({
        "", "--daily",
        "Enable daily file segmentation (output.20251112.csv)",
        false,  // optional
        false,  // no value (flag)
        "",
        ""
    });

    // Parse arguments
    if (!parser.parse(argc, argv)) {
        if (!parser.get_errors().empty()) {
            for (const auto& error : parser.get_errors()) {
                std::cerr << "Error: " << error << std::endl;
            }
            std::cerr << std::endl;
            parser.print_help();
            print_usage_examples();
            return 1;
        }
        return 0; // Help shown
    }

    // Get arguments
    std::string pairs_spec = parser.get("-p");
    std::string output_file = parser.get("-o");
    int flush_interval = std::stoi(parser.get("-f"));
    size_t memory_threshold = std::stoull(parser.get("-m"));
    bool hourly_mode = parser.has("--hourly");
    bool daily_mode = parser.has("--daily");

    // Validate segmentation flags (mutually exclusive)
    if (hourly_mode && daily_mode) {
        std::cerr << "Error: --hourly and --daily cannot be used together" << std::endl;
        return 1;
    }

    // Parse pairs using InputParser from cli_utils
    auto parse_result = cli::InputParser::parse(pairs_spec);

    if (!parse_result.success) {
        std::cerr << "Error: " << parse_result.error_message << std::endl;
        return 1;
    }

    std::vector<std::string> symbols = parse_result.values;

    // Display input source
    std::cout << "Input source: ";
    if (parse_result.type == cli::InputParser::InputType::DIRECT_LIST) {
        std::cout << "Direct list (" << symbols.size() << " pairs)" << std::endl;
    } else if (parse_result.type == cli::InputParser::InputType::CSV_FILE) {
        std::cout << "CSV file: " << parse_result.filepath
                  << " [column: " << parse_result.column_name;
        if (parse_result.limit > 0) {
            std::cout << ", limit: " << parse_result.limit;
        }
        std::cout << "]" << std::endl;
    } else if (parse_result.type == cli::InputParser::InputType::TEXT_FILE) {
        std::cout << "Text file: " << parse_result.filepath;
        if (parse_result.limit > 0) {
            std::cout << " [limit: " << parse_result.limit << "]";
        }
        std::cout << std::endl;
    }
    std::cout << "Output file: " << output_file << std::endl;
    std::cout << "Flush interval: " << flush_interval << " seconds";
    if (flush_interval == 0) {
        std::cout << " (disabled)";
    }
    std::cout << std::endl;
    std::cout << "Memory threshold: ";
    if (memory_threshold == 0) {
        std::cout << "disabled";
    } else {
        double mb = static_cast<double>(memory_threshold) / (1024 * 1024);
        std::cout << std::fixed << std::setprecision(1) << mb << " MB";
    }
    std::cout << std::endl;
    std::cout << "Segmentation: ";
    if (hourly_mode) {
        std::cout << "hourly (output.YYYYMMDD_HH.csv)";
    } else if (daily_mode) {
        std::cout << "daily (output.YYYYMMDD.csv)";
    } else {
        std::cout << "none (single file)";
    }
    std::cout << std::endl;
    std::cout << std::endl;

    // Display configuration
    std::cout << "==================================================" << std::endl;
    std::cout << "Kraken Live Data Retriever - Level 1" << std::endl;
    std::cout << "==================================================" << std::endl;
    std::cout << "Subscribing to " << symbols.size() << " pairs:" << std::endl;
    for (size_t i = 0; i < symbols.size() && i < 10; i++) {
        std::cout << "  - " << symbols[i] << std::endl;
    }
    if (symbols.size() > 10) {
        std::cout << "  ... and " << (symbols.size() - 10) << " more" << std::endl;
    }
    std::cout << std::endl;

    // Setup signal handler
    std::signal(SIGINT, signal_handler);

    // Create WebSocket client
    KrakenWebSocketClientSimdjsonV2 ws_client;
    g_ws_client = &ws_client;

    // Configure flush parameters
    ws_client.set_output_file(output_file);
    ws_client.set_flush_interval(std::chrono::seconds(flush_interval));
    ws_client.set_memory_threshold(memory_threshold);

    // Configure segmentation mode
    if (hourly_mode) {
        ws_client.set_segment_mode(kraken::SegmentMode::HOURLY);
    } else if (daily_mode) {
        ws_client.set_segment_mode(kraken::SegmentMode::DAILY);
    }

    // Setup callbacks
    ws_client.set_update_callback([&](const TickerRecord& record) {
        // Signal new data available
        {
            std::lock_guard<std::mutex> lock(g_cv_mutex);
            g_new_data_available = true;
        }
        g_cv.notify_one();

        // Print real-time update
        std::cout << "[UPDATE] " << record.pair
                  << " | Last: $" << record.last
                  << " | Bid: $" << record.bid
                  << " | Ask: $" << record.ask
                  << " | Vol: " << record.volume
                  << std::endl;
    });

    ws_client.set_connection_callback([](bool connected) {
        std::cout << "[STATUS] WebSocket "
                  << (connected ? "connected" : "disconnected")
                  << std::endl;
    });

    // Start WebSocket client
    if (!ws_client.start(symbols)) {
        std::cerr << "Failed to start WebSocket client" << std::endl;
        return 1;
    }

    std::cout << "Streaming live data... Press Ctrl+C to stop and save." << std::endl;
    std::cout << std::endl;

    // Main event loop
    int update_count = 0;
    auto start_time = std::chrono::steady_clock::now();

    while (g_running && ws_client.is_running()) {
        // Wait for new data or periodic timeout
        {
            std::unique_lock<std::mutex> lock(g_cv_mutex);
            g_cv.wait_for(
                lock,
                std::chrono::seconds(5),
                [] { return g_new_data_available || !g_running; }
            );

            if (!g_running) {
                break;
            }

            if (g_new_data_available) {
                g_new_data_available = false;
                update_count++;
            }
        }

        // Print periodic status
        auto now = std::chrono::steady_clock::now();
        auto elapsed = std::chrono::duration_cast<std::chrono::seconds>(now - start_time).count();

        if (elapsed > 0 && elapsed % 30 == 0) {
            size_t memory_bytes = ws_client.get_current_memory_usage();
            double memory_mb = static_cast<double>(memory_bytes) / (1024 * 1024);

            std::cout << "\n[STATUS] Running time: " << elapsed << "s"
                      << " | Updates: " << update_count
                      << " | Flushes: " << ws_client.get_flush_count()
                      << " | Memory: " << std::fixed << std::setprecision(1) << memory_mb << "MB"
                      << " | Pending: " << ws_client.pending_count();

            // Show segment info if segmentation is enabled
            if (hourly_mode || daily_mode) {
                std::cout << "\n         Current file: " << ws_client.get_current_segment_filename()
                          << " (" << ws_client.get_segment_count() << " files created)";
            }

            std::cout << "\n" << std::endl;
        }
    }

    // Shutdown
    std::cout << "\nFlushing remaining data..." << std::endl;
    ws_client.flush();
    ws_client.stop();

    auto end_time = std::chrono::steady_clock::now();
    auto total_elapsed = std::chrono::duration_cast<std::chrono::seconds>(
        end_time - start_time
    ).count();

    std::cout << "\n==================================================" << std::endl;
    std::cout << "Summary" << std::endl;
    std::cout << "==================================================" << std::endl;
    std::cout << "Pairs monitored: " << symbols.size() << std::endl;
    std::cout << "Total updates: " << update_count << std::endl;
    std::cout << "Total flushes: " << ws_client.get_flush_count() << std::endl;
    std::cout << "Runtime: " << total_elapsed << " seconds" << std::endl;

    if (hourly_mode || daily_mode) {
        std::cout << "Files created: " << ws_client.get_segment_count() << std::endl;
        std::cout << "Output pattern: " << output_file;
        if (hourly_mode) {
            std::cout << " -> *.YYYYMMDD_HH.csv";
        } else {
            std::cout << " -> *.YYYYMMDD.csv";
        }
        std::cout << std::endl;
    } else {
        std::cout << "Output file: " << output_file << std::endl;
    }

    std::cout << "Shutdown complete." << std::endl;

    return 0;
}
