/**
 * Process Level 3 Snapshots
 *
 * Processes raw .jsonl Level 3 order book data to create periodic snapshots with metrics.
 * Reads saved data from retrieve_kraken_live_data_level3 and generates time-series
 * snapshots at specified intervals.
 *
 * Usage:
 *   ./process_level3_snapshots -i level3_raw.jsonl --interval 1s -o snapshots.csv
 *   ./process_level3_snapshots -i level3_raw.jsonl --interval 5s --separate-files
 *   ./process_level3_snapshots -i level3_raw.jsonl --interval 1m --symbol BTC/USD -o btc.csv
 *
 * Output:
 *   CSV file(s) with Level 3 snapshot metrics at specified intervals
 */

#include <iostream>
#include <fstream>
#include <string>
#include <map>
#include <vector>
#include <chrono>
#include <simdjson.h>
#include "cli_utils.hpp"
#include "level3_common.hpp"
#include "level3_state.hpp"
#include "level3_csv_writer.hpp"

using kraken::Level3Record;
using kraken::Level3Order;
using kraken::Level3OrderBookState;
using kraken::Level3SnapshotMetrics;
using kraken::Level3CSVWriter;
using kraken::MultiFileLevel3CSVWriter;

/**
 * Parse interval string (e.g., "1s", "5s", "1m", "1h")
 * Returns interval in seconds, or -1 on error
 */
int parse_interval(const std::string& interval_str) {
    if (interval_str.empty()) {
        return -1;
    }

    // Extract number and unit
    size_t unit_pos = interval_str.find_first_not_of("0123456789");
    if (unit_pos == std::string::npos || unit_pos == 0) {
        std::cerr << "Error: Invalid interval format: " << interval_str << std::endl;
        std::cerr << "Expected format: <number><unit> (e.g., 1s, 5s, 1m, 1h)" << std::endl;
        return -1;
    }

    int value = std::stoi(interval_str.substr(0, unit_pos));
    std::string unit = interval_str.substr(unit_pos);

    if (unit == "s") {
        return value;
    } else if (unit == "m") {
        return value * 60;
    } else if (unit == "h") {
        return value * 3600;
    } else {
        std::cerr << "Error: Unknown time unit: " << unit << std::endl;
        std::cerr << "Supported units: s (seconds), m (minutes), h (hours)" << std::endl;
        return -1;
    }
}

/**
 * Parse timestamp string to Unix epoch seconds
 */
double parse_timestamp(const std::string& timestamp) {
    // Format: "YYYY-MM-DD HH:MM:SS.mmm"
    std::tm tm = {};
    int millisec = 0;

    // Parse: YYYY-MM-DD HH:MM:SS.mmm
    sscanf(timestamp.c_str(), "%d-%d-%d %d:%d:%d.%d",
           &tm.tm_year, &tm.tm_mon, &tm.tm_mday,
           &tm.tm_hour, &tm.tm_min, &tm.tm_sec, &millisec);

    tm.tm_year -= 1900;  // Years since 1900
    tm.tm_mon -= 1;      // Months since January

    std::time_t t = std::mktime(&tm);
    return static_cast<double>(t) + (millisec / 1000.0);
}

/**
 * Parse JSON Lines record to Level3Record
 */
bool parse_jsonl_record(const std::string& line, Level3Record& record) {
    try {
        simdjson::ondemand::parser parser;
        simdjson::padded_string padded(line);
        simdjson::ondemand::document doc = parser.iterate(padded);

        // Parse timestamp
        if (auto ts = doc["timestamp"]; !ts.error()) {
            std::string_view sv = ts.value();
            record.timestamp = std::string(sv);
        }

        // Parse type
        if (auto type = doc["type"]; !type.error()) {
            std::string_view sv = type.value();
            record.type = std::string(sv);
        }

        // Parse data object
        auto data_obj = doc["data"];
        if (data_obj.error()) {
            return false;
        }

        simdjson::ondemand::object data = data_obj.value();

        // Parse symbol
        if (auto symbol = data["symbol"]; !symbol.error()) {
            std::string_view sv = symbol.value();
            record.symbol = std::string(sv);
        }

        // Parse bids (array of order objects)
        if (auto bids = data["bids"]; !bids.error()) {
            simdjson::ondemand::array bids_array = bids.value();
            for (auto bid_value : bids_array) {
                simdjson::ondemand::object bid_obj = bid_value.get_object();

                Level3Order order;

                // Event (for updates)
                if (auto event_field = bid_obj["event"]; !event_field.error()) {
                    std::string_view event_sv = event_field.value();
                    order.event = std::string(event_sv);
                }

                // Order ID
                if (auto order_id = bid_obj["order_id"]; !order_id.error()) {
                    std::string_view id_sv = order_id.value();
                    order.order_id = std::string(id_sv);
                }

                // Limit price
                if (auto limit_price = bid_obj["limit_price"]; !limit_price.error()) {
                    order.limit_price = limit_price.get_double();
                }

                // Order quantity
                if (auto order_qty = bid_obj["order_qty"]; !order_qty.error()) {
                    order.order_qty = order_qty.get_double();
                }

                // Timestamp
                if (auto ts = bid_obj["timestamp"]; !ts.error()) {
                    std::string_view ts_sv = ts.value();
                    order.timestamp = std::string(ts_sv);
                }

                record.bids.push_back(order);
            }
        }

        // Parse asks (same structure as bids)
        if (auto asks = data["asks"]; !asks.error()) {
            simdjson::ondemand::array asks_array = asks.value();
            for (auto ask_value : asks_array) {
                simdjson::ondemand::object ask_obj = ask_value.get_object();

                Level3Order order;

                if (auto event_field = ask_obj["event"]; !event_field.error()) {
                    std::string_view event_sv = event_field.value();
                    order.event = std::string(event_sv);
                }

                if (auto order_id = ask_obj["order_id"]; !order_id.error()) {
                    std::string_view id_sv = order_id.value();
                    order.order_id = std::string(id_sv);
                }

                if (auto limit_price = ask_obj["limit_price"]; !limit_price.error()) {
                    order.limit_price = limit_price.get_double();
                }

                if (auto order_qty = ask_obj["order_qty"]; !order_qty.error()) {
                    order.order_qty = order_qty.get_double();
                }

                if (auto ts = ask_obj["timestamp"]; !ts.error()) {
                    std::string_view ts_sv = ts.value();
                    order.timestamp = std::string(ts_sv);
                }

                record.asks.push_back(order);
            }
        }

        // Parse checksum
        if (auto checksum = data["checksum"]; !checksum.error()) {
            record.checksum = static_cast<uint32_t>(checksum.get_uint64());
        }

        return true;

    } catch (const simdjson::simdjson_error& e) {
        std::cerr << "Error parsing JSON: " << simdjson::error_message(e.error()) << std::endl;
        return false;
    }
}

int main(int argc, char* argv[]) {
    // Setup argument parser
    cli::ArgumentParser parser(argv[0], "Process raw Level 3 order book data to create periodic snapshots");

    parser.add_argument({
        "-i", "--input",
        "Input .jsonl file from retrieve_kraken_live_data_level3",
        true,  // required
        true,  // has value
        "",
        "FILE"
    });

    parser.add_argument({
        "", "--interval",
        "Sampling interval (e.g., 1s, 5s, 1m, 1h)",
        true,  // required
        true,  // has value
        "",
        "TIME"
    });

    parser.add_argument({
        "-o", "--output",
        "Output CSV filename",
        false,  // optional
        true,   // has value
        "level3_snapshots.csv",
        "FILE"
    });

    parser.add_argument({
        "", "--separate-files",
        "Create separate output file per symbol",
        false,  // optional
        false,  // flag
        "",
        ""
    });

    parser.add_argument({
        "", "--symbol",
        "Filter to specific symbol(s) (comma-separated)",
        false,  // optional
        true,   // has value
        "",
        "LIST"
    });

    // Parse arguments
    if (!parser.parse(argc, argv)) {
        if (!parser.get_errors().empty()) {
            for (const auto& error : parser.get_errors()) {
                std::cerr << "Error: " << error << std::endl;
            }
            std::cerr << std::endl;
            parser.print_help();
            return 1;
        }
        return 0; // Help shown
    }

    // Get arguments
    std::string input_file = parser.get("-i");
    std::string interval_str = parser.get("--interval");
    std::string output_file = parser.get("-o");
    bool separate_files = parser.has("--separate-files");
    std::string symbol_filter = parser.get("--symbol");

    // Parse interval
    int interval_seconds = parse_interval(interval_str);
    if (interval_seconds <= 0) {
        return 1;
    }

    // Parse symbol filter if provided
    std::vector<std::string> allowed_symbols;
    if (!symbol_filter.empty()) {
        allowed_symbols = cli::ListParser::parse(symbol_filter, ',');
    }

    // Display configuration
    std::cout << "==================================================" << std::endl;
    std::cout << "Process Level 3 Snapshots" << std::endl;
    std::cout << "==================================================" << std::endl;
    std::cout << "Input file: " << input_file << std::endl;
    std::cout << "Interval: " << interval_str << " (" << interval_seconds << " seconds)" << std::endl;
    if (separate_files) {
        std::cout << "Output mode: Separate files per symbol" << std::endl;
        std::cout << "Output base: " << output_file << std::endl;
    } else {
        std::cout << "Output file: " << output_file << std::endl;
    }
    if (!allowed_symbols.empty()) {
        std::cout << "Symbol filter: ";
        for (size_t i = 0; i < allowed_symbols.size(); i++) {
            if (i > 0) std::cout << ", ";
            std::cout << allowed_symbols[i];
        }
        std::cout << std::endl;
    }
    std::cout << std::endl;

    // Open input file
    std::ifstream infile(input_file);
    if (!infile.is_open()) {
        std::cerr << "Error: Cannot open input file: " << input_file << std::endl;
        return 1;
    }

    // Create output writers
    Level3CSVWriter* single_writer = nullptr;
    MultiFileLevel3CSVWriter* multi_writer = nullptr;

    if (separate_files) {
        multi_writer = new MultiFileLevel3CSVWriter(output_file);
    } else {
        single_writer = new Level3CSVWriter(output_file);
        if (!single_writer->is_open()) {
            std::cerr << "Error: Cannot open output file: " << output_file << std::endl;
            delete single_writer;
            return 1;
        }
    }

    // Maintain Level 3 order book state for each symbol
    std::map<std::string, Level3OrderBookState*> states;

    // Track next sample time for each symbol
    std::map<std::string, double> next_sample_time;

    // Process records
    std::string line;
    int line_num = 0;
    int records_processed = 0;
    int snapshots_written = 0;

    std::cout << "Processing..." << std::endl;

    while (std::getline(infile, line)) {
        line_num++;

        if (line.empty()) {
            continue;
        }

        // Parse record
        Level3Record record;
        if (!parse_jsonl_record(line, record)) {
            std::cerr << "Warning: Failed to parse line " << line_num << std::endl;
            continue;
        }

        // Apply symbol filter
        if (!allowed_symbols.empty()) {
            bool allowed = false;
            for (const auto& sym : allowed_symbols) {
                if (record.symbol == sym) {
                    allowed = true;
                    break;
                }
            }
            if (!allowed) {
                continue;
            }
        }

        // Get or create state for this symbol
        auto it = states.find(record.symbol);
        if (it == states.end()) {
            Level3OrderBookState* new_state = new Level3OrderBookState(record.symbol);
            states[record.symbol] = new_state;
            std::cout << "Initialized Level 3 state for " << record.symbol << std::endl;
            it = states.find(record.symbol);
        }

        Level3OrderBookState* state = it->second;

        // Apply record to state
        if (record.type == "snapshot") {
            state->apply_snapshot(record);
        } else if (record.type == "update") {
            state->apply_update(record);
        }
        records_processed++;

        // Check if we need to take a sample
        double current_time = parse_timestamp(record.timestamp);

        if (next_sample_time.find(record.symbol) == next_sample_time.end()) {
            // First record for this symbol - set next sample time
            next_sample_time[record.symbol] = current_time + interval_seconds;
        }

        if (current_time >= next_sample_time[record.symbol]) {
            // Time to take a sample
            Level3SnapshotMetrics metrics = state->calculate_metrics(record.timestamp);

            // Calculate flow rates (events per interval)
            double interval_time = static_cast<double>(interval_seconds);
            if (interval_time > 0) {
                metrics.order_arrival_rate = metrics.add_events / interval_time;
                metrics.order_cancel_rate = metrics.delete_events / interval_time;
            }

            // Write snapshot
            if (multi_writer) {
                multi_writer->write_snapshot(metrics);
            } else if (single_writer) {
                single_writer->write_snapshot(metrics);
            }

            snapshots_written++;

            // Reset event counters for next interval
            state->reset_event_counters();

            // Update next sample time
            next_sample_time[record.symbol] += interval_seconds;
        }
    }

    infile.close();

    // Flush output
    if (multi_writer) {
        multi_writer->flush_all();
    } else if (single_writer) {
        single_writer->flush();
    }

    // Summary
    std::cout << "\n==================================================" << std::endl;
    std::cout << "Summary" << std::endl;
    std::cout << "==================================================" << std::endl;
    std::cout << "Input records: " << line_num << std::endl;
    std::cout << "Records processed: " << records_processed << std::endl;
    std::cout << "Symbols: " << states.size() << std::endl;
    std::cout << "Snapshots written: " << snapshots_written << std::endl;

    if (separate_files) {
        std::cout << "Files created: " << multi_writer->get_file_count() << std::endl;
        std::cout << "Total snapshots: " << multi_writer->get_total_snapshot_count() << std::endl;
    } else {
        std::cout << "Output file: " << output_file << std::endl;
        std::cout << "Snapshots written: " << single_writer->get_snapshot_count() << std::endl;
    }

    std::cout << "Processing complete." << std::endl;

    // Cleanup
    for (auto& pair : states) {
        delete pair.second;
    }
    if (single_writer) delete single_writer;
    if (multi_writer) delete multi_writer;

    return 0;
}
