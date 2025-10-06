/**
 * JSON Lines Writer for Level 3 Order Book Data - Implementation
 */

#include "level3_jsonl_writer.hpp"
#include <iostream>

namespace kraken {

// ============================================================================
// Level3JsonLinesWriter Implementation
// ============================================================================

Level3JsonLinesWriter::Level3JsonLinesWriter(const std::string& filename, bool append)
    : filename_(filename), record_count_(0) {

    auto mode = append ? (std::ios::out | std::ios::app) : std::ios::out;
    file_.open(filename, mode);

    if (!file_.is_open()) {
        std::cerr << "Error: Cannot open file for writing: " << filename << std::endl;
    }
}

Level3JsonLinesWriter::~Level3JsonLinesWriter() {
    if (file_.is_open()) {
        file_.close();
    }
}

bool Level3JsonLinesWriter::is_open() const {
    return file_.is_open();
}

size_t Level3JsonLinesWriter::get_record_count() const {
    return record_count_;
}

void Level3JsonLinesWriter::flush() {
    if (file_.is_open()) {
        file_.flush();
    }
}

std::string Level3JsonLinesWriter::escape_json_string(const std::string& str) const {
    std::ostringstream oss;
    for (char c : str) {
        switch (c) {
            case '\"':  oss << "\\\""; break;
            case '\\': oss << "\\\\"; break;
            case '\b': oss << "\\b";  break;
            case '\f': oss << "\\f";  break;
            case '\n': oss << "\\n";  break;
            case '\r': oss << "\\r";  break;
            case '\t': oss << "\\t";  break;
            default:   oss << c;       break;
        }
    }
    return oss.str();
}

std::string Level3JsonLinesWriter::order_to_json(const Level3Order& order) const {
    std::ostringstream oss;
    oss << "{";

    // For updates, include event field first
    if (!order.event.empty()) {
        oss << "\"event\":\"" << escape_json_string(order.event) << "\",";
    }

    oss << "\"order_id\":\"" << escape_json_string(order.order_id) << "\",";
    oss << "\"limit_price\":" << std::fixed << std::setprecision(10) << order.limit_price << ",";
    oss << "\"order_qty\":" << std::fixed << std::setprecision(8) << order.order_qty << ",";
    oss << "\"timestamp\":\"" << escape_json_string(order.timestamp) << "\"";
    oss << "}";

    return oss.str();
}

std::string Level3JsonLinesWriter::orders_to_json(const std::vector<Level3Order>& orders) const {
    std::ostringstream oss;
    oss << "[";

    for (size_t i = 0; i < orders.size(); i++) {
        if (i > 0) oss << ",";
        oss << order_to_json(orders[i]);
    }

    oss << "]";
    return oss.str();
}

std::string Level3JsonLinesWriter::record_to_json(const Level3Record& record) const {
    std::ostringstream oss;

    oss << "{";

    // Timestamp
    oss << "\"timestamp\":\"" << escape_json_string(record.timestamp) << "\",";

    // Channel
    oss << "\"channel\":\"level3\",";

    // Type
    oss << "\"type\":\"" << escape_json_string(record.type) << "\",";

    // Data object
    oss << "\"data\":{";
    oss << "\"symbol\":\"" << escape_json_string(record.symbol) << "\",";
    oss << "\"bids\":" << orders_to_json(record.bids) << ",";
    oss << "\"asks\":" << orders_to_json(record.asks) << ",";
    oss << "\"checksum\":" << record.checksum;
    oss << "}";

    oss << "}";

    return oss.str();
}

bool Level3JsonLinesWriter::write_record(const Level3Record& record) {
    if (!file_.is_open()) {
        return false;
    }

    std::string json = record_to_json(record);
    file_ << json << std::endl;

    record_count_++;
    return true;
}

// ============================================================================
// MultiFileLevel3JsonLinesWriter Implementation
// ============================================================================

MultiFileLevel3JsonLinesWriter::MultiFileLevel3JsonLinesWriter(const std::string& base_filename)
    : base_filename_(base_filename) {
}

MultiFileLevel3JsonLinesWriter::~MultiFileLevel3JsonLinesWriter() {
    // Clean up all writers
    for (auto& pair : writers_) {
        delete pair.second;
    }
    writers_.clear();
}

std::string MultiFileLevel3JsonLinesWriter::sanitize_symbol(const std::string& symbol) const {
    std::string result = symbol;

    // Replace / with _
    for (char& c : result) {
        if (c == '/') {
            c = '_';
        }
    }

    return result;
}

std::string MultiFileLevel3JsonLinesWriter::create_filename(const std::string& symbol) const {
    std::string sanitized = sanitize_symbol(symbol);

    // Remove .jsonl extension from base if present
    std::string base = base_filename_;
    if (base.size() > 6 && base.substr(base.size() - 6) == ".jsonl") {
        base = base.substr(0, base.size() - 6);
    }

    return base + "_" + sanitized + ".jsonl";
}

Level3JsonLinesWriter* MultiFileLevel3JsonLinesWriter::get_writer(const std::string& symbol) {
    // Check if writer already exists
    auto it = writers_.find(symbol);
    if (it != writers_.end()) {
        return it->second;
    }

    // Create new writer
    std::string filename = create_filename(symbol);
    Level3JsonLinesWriter* writer = new Level3JsonLinesWriter(filename);

    if (!writer->is_open()) {
        delete writer;
        return nullptr;
    }

    writers_[symbol] = writer;
    return writer;
}

bool MultiFileLevel3JsonLinesWriter::write_record(const Level3Record& record) {
    Level3JsonLinesWriter* writer = get_writer(record.symbol);
    if (!writer) {
        return false;
    }

    return writer->write_record(record);
}

void MultiFileLevel3JsonLinesWriter::flush_all() {
    for (auto& pair : writers_) {
        pair.second->flush();
    }
}

size_t MultiFileLevel3JsonLinesWriter::get_file_count() const {
    return writers_.size();
}

size_t MultiFileLevel3JsonLinesWriter::get_total_record_count() const {
    size_t total = 0;
    for (const auto& pair : writers_) {
        total += pair.second->get_record_count();
    }
    return total;
}

} // namespace kraken
