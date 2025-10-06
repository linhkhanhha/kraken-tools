/**
 * Level 3 Snapshot CSV Writer - Implementation
 */

#include "level3_csv_writer.hpp"
#include <iostream>
#include <sstream>
#include <iomanip>

namespace kraken {

// ============================================================================
// Level3CSVWriter Implementation
// ============================================================================

Level3CSVWriter::Level3CSVWriter(const std::string& filename, bool append)
    : filename_(filename), snapshot_count_(0), header_written_(false) {

    auto mode = append ? (std::ios::out | std::ios::app) : std::ios::out;
    file_.open(filename, mode);

    if (!file_.is_open()) {
        std::cerr << "Error: Cannot open CSV file for writing: " << filename << std::endl;
    }

    // Check if file already has content (for append mode)
    if (append) {
        std::ifstream check(filename);
        if (check.is_open()) {
            std::string first_line;
            if (std::getline(check, first_line) && !first_line.empty()) {
                header_written_ = true;
            }
            check.close();
        }
    }
}

Level3CSVWriter::~Level3CSVWriter() {
    if (file_.is_open()) {
        file_.close();
    }
}

bool Level3CSVWriter::is_open() const {
    return file_.is_open();
}

size_t Level3CSVWriter::get_snapshot_count() const {
    return snapshot_count_;
}

void Level3CSVWriter::flush() {
    if (file_.is_open()) {
        file_.flush();
    }
}

std::string Level3CSVWriter::format_double(double value) const {
    std::ostringstream oss;
    oss << std::setprecision(15) << value;
    std::string result = oss.str();

    // Remove trailing zeros after decimal point
    if (result.find('.') != std::string::npos) {
        result.erase(result.find_last_not_of('0') + 1, std::string::npos);
        if (result.back() == '.') {
            result.pop_back();
        }
    }

    return result;
}

void Level3CSVWriter::write_header() {
    if (header_written_ || !file_.is_open()) {
        return;
    }

    file_ << "timestamp,symbol,"
          << "best_bid,best_bid_qty,best_ask,best_ask_qty,spread,spread_bps,mid_price,"
          << "bid_volume_top10,ask_volume_top10,imbalance,"
          << "depth_10_bps,depth_25_bps,depth_50_bps,"
          << "bid_order_count,ask_order_count,"
          << "bid_orders_at_best,ask_orders_at_best,"
          << "avg_bid_order_size,avg_ask_order_size,"
          << "add_events,modify_events,delete_events,"
          << "order_arrival_rate,order_cancel_rate"
          << std::endl;

    header_written_ = true;
}

bool Level3CSVWriter::write_snapshot(const Level3SnapshotMetrics& metrics) {
    if (!file_.is_open()) {
        return false;
    }

    // Write header if not yet written
    if (!header_written_) {
        write_header();
    }

    // Write data row
    file_ << metrics.timestamp << ","
          << metrics.symbol << ","
          << format_double(metrics.best_bid) << ","
          << format_double(metrics.best_bid_qty) << ","
          << format_double(metrics.best_ask) << ","
          << format_double(metrics.best_ask_qty) << ","
          << format_double(metrics.spread) << ","
          << format_double(metrics.spread_bps) << ","
          << format_double(metrics.mid_price) << ","
          << format_double(metrics.bid_volume_top10) << ","
          << format_double(metrics.ask_volume_top10) << ","
          << format_double(metrics.imbalance) << ","
          << format_double(metrics.depth_10_bps) << ","
          << format_double(metrics.depth_25_bps) << ","
          << format_double(metrics.depth_50_bps) << ","
          << metrics.bid_order_count << ","
          << metrics.ask_order_count << ","
          << metrics.bid_orders_at_best << ","
          << metrics.ask_orders_at_best << ","
          << format_double(metrics.avg_bid_order_size) << ","
          << format_double(metrics.avg_ask_order_size) << ","
          << metrics.add_events << ","
          << metrics.modify_events << ","
          << metrics.delete_events << ","
          << format_double(metrics.order_arrival_rate) << ","
          << format_double(metrics.order_cancel_rate)
          << std::endl;

    snapshot_count_++;
    return true;
}

// ============================================================================
// MultiFileLevel3CSVWriter Implementation
// ============================================================================

MultiFileLevel3CSVWriter::MultiFileLevel3CSVWriter(const std::string& base_filename)
    : base_filename_(base_filename) {
}

MultiFileLevel3CSVWriter::~MultiFileLevel3CSVWriter() {
    // Clean up all writers
    for (auto& pair : writers_) {
        delete pair.second;
    }
    writers_.clear();
}

std::string MultiFileLevel3CSVWriter::sanitize_symbol(const std::string& symbol) const {
    std::string result = symbol;

    // Replace / with _
    for (char& c : result) {
        if (c == '/') {
            c = '_';
        }
    }

    return result;
}

std::string MultiFileLevel3CSVWriter::create_filename(const std::string& symbol) const {
    std::string sanitized = sanitize_symbol(symbol);

    // Remove .csv extension from base if present
    std::string base = base_filename_;
    if (base.size() > 4 && base.substr(base.size() - 4) == ".csv") {
        base = base.substr(0, base.size() - 4);
    }

    return base + "_" + sanitized + ".csv";
}

Level3CSVWriter* MultiFileLevel3CSVWriter::get_writer(const std::string& symbol) {
    // Check if writer already exists
    auto it = writers_.find(symbol);
    if (it != writers_.end()) {
        return it->second;
    }

    // Create new writer
    std::string filename = create_filename(symbol);
    Level3CSVWriter* writer = new Level3CSVWriter(filename);

    if (!writer->is_open()) {
        delete writer;
        return nullptr;
    }

    writers_[symbol] = writer;
    return writer;
}

bool MultiFileLevel3CSVWriter::write_snapshot(const Level3SnapshotMetrics& metrics) {
    Level3CSVWriter* writer = get_writer(metrics.symbol);
    if (!writer) {
        return false;
    }

    return writer->write_snapshot(metrics);
}

void MultiFileLevel3CSVWriter::flush_all() {
    for (auto& pair : writers_) {
        pair.second->flush();
    }
}

size_t MultiFileLevel3CSVWriter::get_file_count() const {
    return writers_.size();
}

size_t MultiFileLevel3CSVWriter::get_total_snapshot_count() const {
    size_t total = 0;
    for (const auto& pair : writers_) {
        total += pair.second->get_snapshot_count();
    }
    return total;
}

} // namespace kraken
