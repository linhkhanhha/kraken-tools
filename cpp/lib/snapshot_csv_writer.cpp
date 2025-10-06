/**
 * Snapshot CSV Writer - Implementation
 */

#include "snapshot_csv_writer.hpp"
#include <iostream>
#include <sstream>
#include <iomanip>

namespace kraken {

// ============================================================================
// SnapshotCSVWriter Implementation
// ============================================================================

SnapshotCSVWriter::SnapshotCSVWriter(const std::string& filename, bool append)
    : filename_(filename), snapshot_count_(0), header_written_(false) {

    auto mode = append ? (std::ios::out | std::ios::app) : std::ios::out;
    file_.open(filename, mode);

    if (!file_.is_open()) {
        std::cerr << "Error: Cannot open file for writing: " << filename << std::endl;
        return;
    }

    // Check if file is empty (need to write header)
    if (!append || file_.tellp() == 0) {
        write_header();
    } else {
        header_written_ = true;
    }
}

SnapshotCSVWriter::~SnapshotCSVWriter() {
    if (file_.is_open()) {
        file_.close();
    }
}

bool SnapshotCSVWriter::is_open() const {
    return file_.is_open();
}

size_t SnapshotCSVWriter::get_snapshot_count() const {
    return snapshot_count_;
}

void SnapshotCSVWriter::flush() {
    if (file_.is_open()) {
        file_.flush();
    }
}

void SnapshotCSVWriter::write_header() {
    if (!file_.is_open()) {
        return;
    }

    file_ << "timestamp,symbol,"
          << "best_bid,best_bid_qty,best_ask,best_ask_qty,"
          << "spread,spread_bps,mid_price,"
          << "bid_volume_top10,ask_volume_top10,imbalance,"
          << "depth_10_bps,depth_25_bps,depth_50_bps"
          << std::endl;

    header_written_ = true;
}

std::string SnapshotCSVWriter::format_double(double value) const {
    // Use adaptive precision - let std::to_string handle it, then remove trailing zeros
    std::ostringstream oss;
    oss << std::setprecision(15) << value;
    std::string result = oss.str();

    // Remove trailing zeros after decimal point
    if (result.find('.') != std::string::npos) {
        // Remove trailing zeros
        result.erase(result.find_last_not_of('0') + 1, std::string::npos);
        // Remove trailing decimal point if no decimals left
        if (result.back() == '.') {
            result.pop_back();
        }
    }

    return result;
}

bool SnapshotCSVWriter::write_snapshot(const SnapshotMetrics& metrics) {
    if (!file_.is_open()) {
        return false;
    }

    // Write data row with adaptive precision
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
          << format_double(metrics.depth_50_bps)
          << std::endl;

    snapshot_count_++;
    return true;
}

// ============================================================================
// MultiFileSnapshotCSVWriter Implementation
// ============================================================================

MultiFileSnapshotCSVWriter::MultiFileSnapshotCSVWriter(const std::string& base_filename)
    : base_filename_(base_filename) {
}

MultiFileSnapshotCSVWriter::~MultiFileSnapshotCSVWriter() {
    // Clean up all writers
    for (auto& pair : writers_) {
        delete pair.second;
    }
    writers_.clear();
}

std::string MultiFileSnapshotCSVWriter::sanitize_symbol(const std::string& symbol) const {
    std::string result = symbol;

    // Replace / with _
    for (char& c : result) {
        if (c == '/') {
            c = '_';
        }
    }

    return result;
}

std::string MultiFileSnapshotCSVWriter::create_filename(const std::string& symbol) const {
    std::string sanitized = sanitize_symbol(symbol);

    // Remove .csv extension from base if present
    std::string base = base_filename_;
    if (base.size() > 4 && base.substr(base.size() - 4) == ".csv") {
        base = base.substr(0, base.size() - 4);
    }

    return base + "_" + sanitized + ".csv";
}

SnapshotCSVWriter* MultiFileSnapshotCSVWriter::get_writer(const std::string& symbol) {
    // Check if writer already exists
    auto it = writers_.find(symbol);
    if (it != writers_.end()) {
        return it->second;
    }

    // Create new writer
    std::string filename = create_filename(symbol);
    SnapshotCSVWriter* writer = new SnapshotCSVWriter(filename);

    if (!writer->is_open()) {
        delete writer;
        return nullptr;
    }

    writers_[symbol] = writer;
    return writer;
}

bool MultiFileSnapshotCSVWriter::write_snapshot(const SnapshotMetrics& metrics) {
    SnapshotCSVWriter* writer = get_writer(metrics.symbol);
    if (!writer) {
        return false;
    }

    return writer->write_snapshot(metrics);
}

void MultiFileSnapshotCSVWriter::flush_all() {
    for (auto& pair : writers_) {
        pair.second->flush();
    }
}

size_t MultiFileSnapshotCSVWriter::get_file_count() const {
    return writers_.size();
}

size_t MultiFileSnapshotCSVWriter::get_total_snapshot_count() const {
    size_t total = 0;
    for (const auto& pair : writers_) {
        total += pair.second->get_snapshot_count();
    }
    return total;
}

} // namespace kraken
