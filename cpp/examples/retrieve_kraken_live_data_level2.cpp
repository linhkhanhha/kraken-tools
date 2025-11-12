/**
 * Kraken Live Data Retriever - Level 2 (Order Book Data)
 *
 * A production-ready tool for retrieving real-time order book data from Kraken.
 * Supports flexible pair input via command-line arguments:
 * - Direct pair list: -p "BTC/USD,ETH/USD,SOL/USD"
 * - CSV file: -p /path/to/file.csv:column_name:limit
 * - Text file: -p /path/to/file.txt:limit
 *
 * Usage:
 *   ./retrieve_kraken_live_data_level2 -p "BTC/USD"
 *   ./retrieve_kraken_live_data_level2 -p "BTC/USD,ETH/USD" -d 25 --show-top
 *   ./retrieve_kraken_live_data_level2 -p pairs.txt:10 --separate-files
 *   ./retrieve_kraken_live_data_level2 -p "BTC/USD" --show-book -v
 *
 * Output:
 *   Saves order book data to .jsonl format (JSON Lines)
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
#include "kraken_book_client.hpp"
#include "cli_utils.hpp"
#include "orderbook_common.hpp"
#include "jsonl_writer.hpp"

using kraken::KrakenBookClient;
using kraken::OrderBookRecord;
using kraken::OrderBookStats;
using kraken::OrderBookDisplay;
using kraken::JsonLinesWriter;
using kraken::MultiFileJsonLinesWriter;

// Global state
KrakenBookClient* g_book_client = nullptr;
std::atomic<bool> g_running{true};
std::mutex g_cv_mutex;
std::condition_variable g_cv;
bool g_new_data_available = false;

// Display options
bool g_show_updates = false;
bool g_show_top = false;
bool g_show_book = false;

// Output writers
JsonLinesWriter* g_single_writer = nullptr;
MultiFileJsonLinesWriter* g_multi_writer = nullptr;

void signal_handler(int) {
    std::cout << "\n\nShutting down..." << std::endl;
    g_running = false;
    g_cv.notify_all();
}

void print_usage_examples() {
    std::cout << std::endl;
    std::cout << "Examples:" << std::endl;
    std::cout << "  1. Minimal (fastest):" << std::endl;
    std::cout << "     -p \"BTC/USD,ETH/USD\"" << std::endl;
    std::cout << std::endl;
    std::cout << "  2. With depth and display:" << std::endl;
    std::cout << "     -p \"BTC/USD\" -d 25 --show-top" << std::endl;
    std::cout << std::endl;
    std::cout << "  3. From file, separate outputs:" << std::endl;
    std::cout << "     -p tickers.txt:10 --separate-files" << std::endl;
    std::cout << std::endl;
    std::cout << "  4. Full monitoring (single pair only):" << std::endl;
    std::cout << "     -p \"BTC/USD\" --show-book -v --show-top" << std::endl;
    std::cout << std::endl;
    std::cout << "Display Options:" << std::endl;
    std::cout << "  (default)  - Minimal counters (fastest)" << std::endl;
    std::cout << "  -v         - Show update details" << std::endl;
    std::cout << "  --show-top - Show top-of-book" << std::endl;
    std::cout << "  --show-book - Show full order book (single pair)" << std::endl;
    std::cout << std::endl;
}

int main(int argc, char* argv[]) {
    // Setup argument parser
    cli::ArgumentParser parser(argv[0], "Retrieve real-time Level 2 order book data from Kraken");

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
        "Order book depth (10, 25, 100, 500, 1000)",
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
        "kraken_orderbook.jsonl",
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
        "", "--skip-validation",
        "Skip checksum validation (faster)",
        false,  // optional
        false,  // flag
        "",
        ""
    });

    parser.add_argument({
        "-v", "--show-updates",
        "Show update details",
        false,  // optional
        false,  // flag
        "",
        ""
    });

    parser.add_argument({
        "", "--show-top",
        "Show top-of-book",
        false,  // optional
        false,  // flag
        "",
        ""
    });

    parser.add_argument({
        "", "--show-book",
        "Show full order book (single pair only)",
        false,  // optional
        false,  // flag
        "",
        ""
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
        "10485760",  // 10 MB
        "BYTES"
    });

    parser.add_argument({
        "", "--hourly",
        "Enable hourly file segmentation (output.20251112_10.jsonl)",
        false,  // optional
        false,  // flag
        "",
        ""
    });

    parser.add_argument({
        "", "--daily",
        "Enable daily file segmentation (output.20251112.jsonl)",
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
    bool skip_validation = parser.has("--skip-validation");
    g_show_updates = parser.has("-v") || parser.has("--show-updates");
    g_show_top = parser.has("--show-top");
    g_show_book = parser.has("--show-book");

    // Flush and segmentation arguments
    int flush_interval = std::stoi(parser.get("-f"));
    size_t memory_threshold = std::stoull(parser.get("-m"));
    bool hourly_mode = parser.has("--hourly");
    bool daily_mode = parser.has("--daily");

    // Validate segmentation flags
    if (hourly_mode && daily_mode) {
        std::cerr << "Error: --hourly and --daily cannot be used together" << std::endl;
        return 1;
    }

    // Parse depth
    int depth = std::stoi(depth_str);
    if (depth != 10 && depth != 25 && depth != 100 && depth != 500 && depth != 1000) {
        std::cerr << "Error: Depth must be one of: 10, 25, 100, 500, 1000" << std::endl;
        return 1;
    }

    // Parse pairs using InputParser from cli_utils
    auto parse_result = cli::InputParser::parse(pairs_spec);

    if (!parse_result.success) {
        std::cerr << "Error: " << parse_result.error_message << std::endl;
        return 1;
    }

    std::vector<std::string> symbols = parse_result.values;

    // Validate --show-book with multiple pairs
    if (g_show_book && symbols.size() > 1) {
        std::cerr << "Error: --show-book can only be used with a single pair" << std::endl;
        std::cerr << "You specified " << symbols.size() << " pairs" << std::endl;
        return 1;
    }

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
    std::cout << "Kraken Live Data Retriever - Level 2" << std::endl;
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
    std::cout << "  Checksum validation: " << (skip_validation ? "disabled" : "enabled") << std::endl;

    // Flush configuration
    std::cout << "  Flush interval: ";
    if (flush_interval > 0) {
        std::cout << flush_interval << " seconds";
    } else {
        std::cout << "disabled";
    }
    std::cout << std::endl;

    std::cout << "  Memory threshold: ";
    if (memory_threshold > 0) {
        std::cout << std::fixed << std::setprecision(1)
                  << (memory_threshold / 1024.0 / 1024.0) << " MB";
    } else {
        std::cout << "disabled";
    }
    std::cout << std::endl;

    // Segmentation
    if (hourly_mode || daily_mode) {
        std::cout << "  Segmentation: ";
        if (hourly_mode) {
            std::cout << "hourly (output.YYYYMMDD_HH.jsonl)";
        } else {
            std::cout << "daily (output.YYYYMMDD.jsonl)";
        }
        std::cout << std::endl;
    }

    std::cout << "  Display mode: ";
    if (g_show_book) {
        std::cout << "Full order book";
    } else if (g_show_top) {
        std::cout << "Top-of-book";
    } else if (g_show_updates) {
        std::cout << "Update details";
    } else {
        std::cout << "Minimal counters";
    }
    std::cout << std::endl;
    std::cout << std::endl;

    // Setup signal handler
    std::signal(SIGINT, signal_handler);

    // Create output writers
    if (separate_files) {
        g_multi_writer = new MultiFileJsonLinesWriter(output_file);

        // Configure flush and segmentation
        g_multi_writer->set_flush_interval(std::chrono::seconds(flush_interval));
        g_multi_writer->set_memory_threshold(memory_threshold);

        if (hourly_mode) {
            g_multi_writer->set_segment_mode(kraken::SegmentMode::HOURLY);
        } else if (daily_mode) {
            g_multi_writer->set_segment_mode(kraken::SegmentMode::DAILY);
        }
    } else {
        g_single_writer = new JsonLinesWriter(output_file);

        // Configure flush and segmentation
        g_single_writer->set_flush_interval(std::chrono::seconds(flush_interval));
        g_single_writer->set_memory_threshold(memory_threshold);

        if (hourly_mode) {
            g_single_writer->set_segment_mode(kraken::SegmentMode::HOURLY);
        } else if (daily_mode) {
            g_single_writer->set_segment_mode(kraken::SegmentMode::DAILY);
        }

        // Check if file is open after configuration
        // (file opens in set_segment_mode or on first write)
        if (hourly_mode || daily_mode) {
            // File should be open after set_segment_mode
            if (!g_single_writer->is_open()) {
                std::cerr << "Error: Failed to open segment file" << std::endl;
                delete g_single_writer;
                return 1;
            }
        }
        // For non-segmented mode, file will open on first write
    }

    // Create WebSocket client
    KrakenBookClient book_client(depth, !skip_validation);
    g_book_client = &book_client;

    // Setup callbacks
    book_client.set_update_callback([&](const OrderBookRecord& record) {
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
        if (g_show_book) {
            // Full order book display (single pair only)
            OrderBookDisplay::show_full_book(record, depth);
        } else if (g_show_top) {
            // Top-of-book display
            OrderBookDisplay::show_top_of_book(record);
        } else if (g_show_updates) {
            // Update details
            OrderBookDisplay::show_update_details(record, "[UPDATE]");
        }
        // Minimal mode: handled in periodic status below
    });

    book_client.set_connection_callback([](bool connected) {
        std::cout << "[STATUS] WebSocket "
                  << (connected ? "connected" : "disconnected")
                  << std::endl;
    });

    // Start WebSocket client
    if (!book_client.start(symbols)) {
        std::cerr << "Failed to start WebSocket client" << std::endl;
        if (g_single_writer) delete g_single_writer;
        if (g_multi_writer) delete g_multi_writer;
        return 1;
    }

    std::cout << "Streaming live order book data... Press Ctrl+C to stop and save." << std::endl;
    std::cout << std::endl;

    // Main event loop
    int update_count = 0;
    auto start_time = std::chrono::steady_clock::now();
    auto last_status_time = start_time;

    while (g_running && book_client.is_running()) {
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

        // Print periodic status (minimal mode only)
        auto now = std::chrono::steady_clock::now();
        auto elapsed_since_status = std::chrono::duration_cast<std::chrono::seconds>(
            now - last_status_time
        ).count();

        if (!g_show_book && !g_show_top && !g_show_updates && elapsed_since_status >= 10) {
            // Minimal mode: show counters every 10 seconds
            auto stats = book_client.get_stats();
            OrderBookDisplay::show_minimal(stats);
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

    book_client.stop();

    auto end_time = std::chrono::steady_clock::now();
    auto total_elapsed = std::chrono::duration_cast<std::chrono::seconds>(
        end_time - start_time
    ).count();

    // Get final statistics
    auto final_stats = book_client.get_stats();

    std::cout << "\n==================================================" << std::endl;
    std::cout << "Summary" << std::endl;
    std::cout << "==================================================" << std::endl;
    std::cout << "Pairs monitored: " << symbols.size() << std::endl;

    // Calculate totals
    int total_snapshots = 0;
    int total_updates = 0;
    for (const auto& pair : final_stats) {
        total_snapshots += pair.second.snapshot_count;
        total_updates += pair.second.update_count;
    }

    std::cout << "Total snapshots: " << total_snapshots << std::endl;
    std::cout << "Total updates: " << total_updates << std::endl;
    std::cout << "Total messages: " << (total_snapshots + total_updates) << std::endl;
    std::cout << "Runtime: " << total_elapsed << " seconds" << std::endl;

    if (separate_files) {
        std::cout << "Files created: " << g_multi_writer->get_file_count() << std::endl;
        std::cout << "Total records: " << g_multi_writer->get_total_record_count() << std::endl;
        std::cout << "Total flushes: " << g_multi_writer->get_total_flush_count() << std::endl;
        if (hourly_mode || daily_mode) {
            std::cout << "Total segments: " << g_multi_writer->get_total_segment_count() << std::endl;
        }
    } else {
        std::cout << "Output file: " << output_file << std::endl;
        std::cout << "Records written: " << g_single_writer->get_record_count() << std::endl;
        std::cout << "Flushes: " << g_single_writer->get_flush_count() << std::endl;
        if (hourly_mode || daily_mode) {
            std::cout << "Segments created: " << g_single_writer->get_segment_count() << std::endl;
            std::cout << "Final segment: " << g_single_writer->get_current_segment_filename() << std::endl;
        }
    }

    std::cout << "Shutdown complete." << std::endl;

    // Cleanup
    if (g_single_writer) delete g_single_writer;
    if (g_multi_writer) delete g_multi_writer;

    return 0;
}
