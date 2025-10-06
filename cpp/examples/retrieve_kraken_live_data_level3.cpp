/**
 * Kraken Live Data Retriever - Level 3 (Individual Order Data)
 *
 * A production-ready tool for retrieving real-time Level 3 order book data.
 * Level 3 provides individual order granularity with order_id, requiring authentication.
 *
 * Usage:
 *   export KRAKEN_WS_TOKEN="your_token"
 *   ./retrieve_kraken_live_data_level3 -p "BTC/USD"
 *   ./retrieve_kraken_live_data_level3 -p "BTC/USD,ETH/USD" -d 100 -v --show-top
 *   ./retrieve_kraken_live_data_level3 -p "BTC/USD" --token-file ~/.kraken/ws_token
 *
 * Output:
 *   Saves Level 3 order data to .jsonl format
 */

#include <iostream>
#include <csignal>
#include <chrono>
#include <atomic>
#include <vector>
#include <string>
#include <mutex>
#include <condition_variable>
#include "kraken_level3_client.hpp"
#include "cli_utils.hpp"
#include "level3_common.hpp"
#include "level3_jsonl_writer.hpp"

using kraken::KrakenLevel3Client;
using kraken::Level3Record;
using kraken::Level3Stats;
using kraken::Level3Display;
using kraken::Level3JsonLinesWriter;
using kraken::MultiFileLevel3JsonLinesWriter;

// Global state
KrakenLevel3Client* g_level3_client = nullptr;
std::atomic<bool> g_running{true};
std::mutex g_cv_mutex;
std::condition_variable g_cv;
bool g_new_data_available = false;

// Display options
bool g_show_events = false;
bool g_show_top = false;
bool g_show_orders = false;

// Output writers
Level3JsonLinesWriter* g_single_writer = nullptr;
MultiFileLevel3JsonLinesWriter* g_multi_writer = nullptr;

void signal_handler(int) {
    std::cout << "\n\nShutting down..." << std::endl;
    g_running = false;
    g_cv.notify_all();
}

void print_usage_examples() {
    std::cout << std::endl;
    std::cout << "Authentication Setup:" << std::endl;
    std::cout << "  Option 1: Environment variable (recommended)" << std::endl;
    std::cout << "    export KRAKEN_WS_TOKEN=\"your_token_here\"" << std::endl;
    std::cout << std::endl;
    std::cout << "  Option 2: Token file" << std::endl;
    std::cout << "    echo \"your_token\" > ~/.kraken/ws_token" << std::endl;
    std::cout << "    chmod 600 ~/.kraken/ws_token" << std::endl;
    std::cout << "    --token-file ~/.kraken/ws_token" << std::endl;
    std::cout << std::endl;
    std::cout << "  Option 3: Direct (for testing)" << std::endl;
    std::cout << "    --token \"your_token_here\"" << std::endl;
    std::cout << std::endl;
    std::cout << "Examples:" << std::endl;
    std::cout << "  1. Minimal (fastest):" << std::endl;
    std::cout << "     -p \"BTC/USD\"" << std::endl;
    std::cout << std::endl;
    std::cout << "  2. With event display:" << std::endl;
    std::cout << "     -p \"BTC/USD\" -v" << std::endl;
    std::cout << std::endl;
    std::cout << "  3. Top-of-book with order details:" << std::endl;
    std::cout << "     -p \"BTC/USD\" --show-top" << std::endl;
    std::cout << std::endl;
    std::cout << "  4. Live order feed (verbose):" << std::endl;
    std::cout << "     -p \"BTC/USD\" --show-orders" << std::endl;
    std::cout << std::endl;
    std::cout << "  5. High depth with token file:" << std::endl;
    std::cout << "     -p \"BTC/USD\" -d 100 --token-file ~/.kraken/ws_token" << std::endl;
    std::cout << std::endl;
}

int main(int argc, char* argv[]) {
    // Setup argument parser
    cli::ArgumentParser parser(argv[0], "Retrieve real-time Level 3 order book data from Kraken");

    parser.add_argument({
        "-p", "--pairs",
        "Pairs specification (direct list, CSV file, or text file)",
        true,  // required
        true,  // has value
        "",
        "SPEC"
    });

    parser.add_argument({
        "-d", "--depth",
        "Order book depth (10, 100, 1000)",
        false,  // optional
        true,   // has value
        "10",
        "NUM"
    });

    parser.add_argument({
        "-o", "--output",
        "Output filename (JSON Lines format)",
        false,  // optional
        true,   // has value
        "kraken_level3.jsonl",
        "FILE"
    });

    parser.add_argument({
        "", "--separate-files",
        "Create separate file per symbol",
        false,  // optional
        false,  // flag
        "",
        ""
    });

    parser.add_argument({
        "", "--token",
        "Authentication token (highest priority)",
        false,  // optional
        true,   // has value
        "",
        "TOKEN"
    });

    parser.add_argument({
        "", "--token-file",
        "File containing authentication token",
        false,  // optional
        true,   // has value
        "",
        "FILE"
    });

    parser.add_argument({
        "-v", "--show-events",
        "Show event counts (add/modify/delete)",
        false,  // optional
        false,  // flag
        "",
        ""
    });

    parser.add_argument({
        "", "--show-top",
        "Show top-of-book with order details",
        false,  // optional
        false,  // flag
        "",
        ""
    });

    parser.add_argument({
        "", "--show-orders",
        "Show live order event feed (verbose)",
        false,  // optional
        false,  // flag
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
    std::string depth_str = parser.get("-d");
    std::string output_file = parser.get("-o");
    bool separate_files = parser.has("--separate-files");
    std::string token_param = parser.get("--token");
    std::string token_file = parser.get("--token-file");
    g_show_events = parser.has("-v") || parser.has("--show-events");
    g_show_top = parser.has("--show-top");
    g_show_orders = parser.has("--show-orders");

    // Parse depth
    int depth = std::stoi(depth_str);
    // Note: We don't validate depth here, let server reject if invalid

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

    if (separate_files) {
        std::cout << "Output mode: Separate files per symbol" << std::endl;
        std::cout << "Output base: " << output_file << std::endl;
    } else {
        std::cout << "Output file: " << output_file << std::endl;
    }
    std::cout << std::endl;

    // Display configuration
    std::cout << "==================================================" << std::endl;
    std::cout << "Kraken Live Data Retriever - Level 3" << std::endl;
    std::cout << "==================================================" << std::endl;
    std::cout << "Subscribing to " << symbols.size() << " pairs:" << std::endl;
    for (size_t i = 0; i < symbols.size() && i < 10; i++) {
        std::cout << "  - " << symbols[i] << std::endl;
    }
    if (symbols.size() > 10) {
        std::cout << "  ... and " << (symbols.size() - 10) << " more" << std::endl;
    }
    std::cout << std::endl;
    std::cout << "Configuration:" << std::endl;
    std::cout << "  Depth: " << depth << " levels" << std::endl;
    std::cout << "  Display mode: ";
    if (g_show_orders) {
        std::cout << "Live order feed (verbose)";
    } else if (g_show_top) {
        std::cout << "Top-of-book with orders";
    } else if (g_show_events) {
        std::cout << "Event counts";
    } else {
        std::cout << "Minimal counters";
    }
    std::cout << std::endl;
    std::cout << std::endl;

    // Setup signal handler
    std::signal(SIGINT, signal_handler);

    // Create output writers
    if (separate_files) {
        g_multi_writer = new MultiFileLevel3JsonLinesWriter(output_file);
    } else {
        g_single_writer = new Level3JsonLinesWriter(output_file);
        if (!g_single_writer->is_open()) {
            std::cerr << "Error: Failed to open output file: " << output_file << std::endl;
            delete g_single_writer;
            return 1;
        }
    }

    // Create WebSocket client
    KrakenLevel3Client level3_client(depth);
    g_level3_client = &level3_client;

    // Setup authentication (priority: --token > --token-file > env var)
    bool token_set = false;
    if (!token_param.empty()) {
        std::cout << "Using token from --token parameter" << std::endl;
        token_set = level3_client.set_token(token_param);
    } else if (!token_file.empty()) {
        std::cout << "Using token from file: " << token_file << std::endl;
        token_set = level3_client.set_token_from_file(token_file);
    } else {
        std::cout << "Using token from KRAKEN_WS_TOKEN environment variable" << std::endl;
        token_set = level3_client.set_token_from_env();
    }

    if (!token_set) {
        std::cerr << "Error: No valid authentication token found" << std::endl;
        std::cerr << std::endl;
        std::cerr << "Level 3 requires authentication. Please set token via:" << std::endl;
        std::cerr << "  1. --token parameter" << std::endl;
        std::cerr << "  2. --token-file parameter" << std::endl;
        std::cerr << "  3. KRAKEN_WS_TOKEN environment variable" << std::endl;
        std::cerr << std::endl;
        print_usage_examples();
        if (g_single_writer) delete g_single_writer;
        if (g_multi_writer) delete g_multi_writer;
        return 1;
    }

    std::cout << "Authentication: Token configured" << std::endl;
    std::cout << std::endl;

    // Setup callbacks
    level3_client.set_update_callback([&](const Level3Record& record) {
        // Write to file
        if (g_multi_writer) {
            g_multi_writer->write_record(record);
        } else if (g_single_writer) {
            g_single_writer->write_record(record);
        }

        // Signal new data available
        {
            std::lock_guard<std::mutex> lock(g_cv_mutex);
            g_new_data_available = true;
        }
        g_cv.notify_one();

        // Display based on flags
        if (g_show_orders) {
            // Show individual order events
            for (const auto& order : record.bids) {
                if (!order.event.empty()) {  // Only show events for updates
                    Level3Display::show_order_event(order, record.symbol, true);
                }
            }
            for (const auto& order : record.asks) {
                if (!order.event.empty()) {
                    Level3Display::show_order_event(order, record.symbol, false);
                }
            }
        } else if (g_show_top) {
            // Top-of-book display
            Level3Display::show_top_of_book(record);
        }
        // Event counts and minimal modes: handled in periodic status below
    });

    level3_client.set_connection_callback([](bool connected) {
        std::cout << "[STATUS] WebSocket "
                  << (connected ? "connected" : "disconnected")
                  << std::endl;
    });

    level3_client.set_error_callback([](const std::string& error) {
        std::cerr << "[ERROR] " << error << std::endl;
    });

    // Start WebSocket client
    if (!level3_client.start(symbols)) {
        std::cerr << "Failed to start WebSocket client" << std::endl;
        if (g_single_writer) delete g_single_writer;
        if (g_multi_writer) delete g_multi_writer;
        return 1;
    }

    std::cout << "Streaming Level 3 order data... Press Ctrl+C to stop and save." << std::endl;
    std::cout << std::endl;

    // Main event loop
    int update_count = 0;
    auto start_time = std::chrono::steady_clock::now();
    auto last_status_time = start_time;

    while (g_running && level3_client.is_running()) {
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
        auto elapsed_since_status = std::chrono::duration_cast<std::chrono::seconds>(
            now - last_status_time
        ).count();

        if (!g_show_orders && elapsed_since_status >= 10) {
            // Get statistics
            auto stats = level3_client.get_stats();

            if (g_show_events) {
                // Show event counts
                Level3Display::show_event_counts(stats);
            } else if (!g_show_top) {
                // Minimal mode: show counters
                Level3Display::show_minimal(stats);
            }

            last_status_time = now;
        }
    }

    // Shutdown
    std::cout << "\nFlushing data..." << std::endl;
    if (g_multi_writer) {
        g_multi_writer->flush_all();
    } else if (g_single_writer) {
        g_single_writer->flush();
    }

    level3_client.stop();

    auto end_time = std::chrono::steady_clock::now();
    auto total_elapsed = std::chrono::duration_cast<std::chrono::seconds>(
        end_time - start_time
    ).count();

    // Get final statistics
    auto final_stats = level3_client.get_stats();

    std::cout << "\n==================================================" << std::endl;
    std::cout << "Summary" << std::endl;
    std::cout << "==================================================" << std::endl;
    std::cout << "Pairs monitored: " << symbols.size() << std::endl;

    // Calculate totals
    int total_snapshots = 0;
    int total_updates = 0;
    int total_adds = 0;
    int total_modifies = 0;
    int total_deletes = 0;
    for (const auto& pair : final_stats) {
        total_snapshots += pair.second.snapshot_count;
        total_updates += pair.second.update_count;
        total_adds += pair.second.add_events;
        total_modifies += pair.second.modify_events;
        total_deletes += pair.second.delete_events;
    }

    std::cout << "Total snapshots: " << total_snapshots << std::endl;
    std::cout << "Total updates: " << total_updates << std::endl;
    std::cout << "Total messages: " << (total_snapshots + total_updates) << std::endl;
    std::cout << "Order events:" << std::endl;
    std::cout << "  Add: " << total_adds << std::endl;
    std::cout << "  Modify: " << total_modifies << std::endl;
    std::cout << "  Delete: " << total_deletes << std::endl;
    std::cout << "Runtime: " << total_elapsed << " seconds" << std::endl;

    if (separate_files) {
        std::cout << "Files created: " << g_multi_writer->get_file_count() << std::endl;
        std::cout << "Total records: " << g_multi_writer->get_total_record_count() << std::endl;
    } else {
        std::cout << "Output file: " << output_file << std::endl;
        std::cout << "Records written: " << g_single_writer->get_record_count() << std::endl;
    }

    std::cout << "Shutdown complete." << std::endl;

    // Cleanup
    if (g_single_writer) delete g_single_writer;
    if (g_multi_writer) delete g_multi_writer;

    return 0;
}
