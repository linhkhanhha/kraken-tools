/**
 * JSON Lines Writer for Order Book Data - Implementation
 */

#include "jsonl_writer.hpp"
#include <iostream>

namespace kraken {

// ============================================================================
// JsonLinesWriter Implementation
// ============================================================================

JsonLinesWriter::JsonLinesWriter(const std::string& filename, bool append)
    : filename_(filename), record_count_(0) {

    auto mode = append ? (std::ios::out | std::ios::app) : std::ios::out;
    file_.open(filename, mode);

    if (!file_.is_open()) {
        std::cerr << "Error: Cannot open file for writing: " << filename << std::endl;
    }
}

JsonLinesWriter::~JsonLinesWriter() {
    if (file_.is_open()) {
        file_.close();
    }
}

bool JsonLinesWriter::is_open() const {
    return file_.is_open();
}

size_t JsonLinesWriter::get_record_count() const {
    return record_count_;
}

void JsonLinesWriter::flush() {
    if (file_.is_open()) {
        file_.flush();
    }
}

std::string JsonLinesWriter::escape_json_string(const std::string& str) const {
    std::ostringstream oss;
    for (char c : str) {
        switch (c) {
            case '"':  oss << "\\\""; break;
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

std::string JsonLinesWriter::price_levels_to_json(const std::vector<PriceLevel>& levels) const {
    std::ostringstream oss;
    oss << "[";

    for (size_t i = 0; i < levels.size(); i++) {
        if (i > 0) oss << ",";
        oss << "["
            << std::fixed << std::setprecision(10) << levels[i].price << ","
            << std::fixed << std::setprecision(8) << levels[i].quantity
            << "]";
    }

    oss << "]";
    return oss.str();
}

std::string JsonLinesWriter::record_to_json(const OrderBookRecord& record) const {
    std::ostringstream oss;

    oss << "{";

    // Timestamp
    oss << "\"timestamp\":\"" << escape_json_string(record.timestamp) << "\",";

    // Channel
    oss << "\"channel\":\"book\",";

    // Type
    oss << "\"type\":\"" << escape_json_string(record.type) << "\",";

    // Data object
    oss << "\"data\":{";
    oss << "\"symbol\":\"" << escape_json_string(record.symbol) << "\",";
    oss << "\"bids\":" << price_levels_to_json(record.bids) << ",";
    oss << "\"asks\":" << price_levels_to_json(record.asks) << ",";
    oss << "\"checksum\":" << record.checksum;
    oss << "}";

    oss << "}";

    return oss.str();
}

bool JsonLinesWriter::write_record(const OrderBookRecord& record) {
    if (!file_.is_open()) {
        return false;
    }

    std::string json = record_to_json(record);
    file_ << json << std::endl;

    record_count_++;
    return true;
}

// ============================================================================
// MultiFileJsonLinesWriter Implementation
// ============================================================================

MultiFileJsonLinesWriter::MultiFileJsonLinesWriter(const std::string& base_filename)
    : base_filename_(base_filename) {
}

MultiFileJsonLinesWriter::~MultiFileJsonLinesWriter() {
    // Clean up all writers
    for (auto& pair : writers_) {
        delete pair.second;
    }
    writers_.clear();
}

std::string MultiFileJsonLinesWriter::sanitize_symbol(const std::string& symbol) const {
    std::string result = symbol;

    // Replace / with _
    for (char& c : result) {
        if (c == '/') {
            c = '_';
        }
    }

    return result;
}

std::string MultiFileJsonLinesWriter::create_filename(const std::string& symbol) const {
    std::string sanitized = sanitize_symbol(symbol);

    // Remove .jsonl extension from base if present
    std::string base = base_filename_;
    if (base.size() > 6 && base.substr(base.size() - 6) == ".jsonl") {
        base = base.substr(0, base.size() - 6);
    }

    return base + "_" + sanitized + ".jsonl";
}

JsonLinesWriter* MultiFileJsonLinesWriter::get_writer(const std::string& symbol) {
    // Check if writer already exists
    auto it = writers_.find(symbol);
    if (it != writers_.end()) {
        return it->second;
    }

    // Create new writer
    std::string filename = create_filename(symbol);
    JsonLinesWriter* writer = new JsonLinesWriter(filename);

    if (!writer->is_open()) {
        delete writer;
        return nullptr;
    }

    writers_[symbol] = writer;
    return writer;
}

bool MultiFileJsonLinesWriter::write_record(const OrderBookRecord& record) {
    JsonLinesWriter* writer = get_writer(record.symbol);
    if (!writer) {
        return false;
    }

    return writer->write_record(record);
}

void MultiFileJsonLinesWriter::flush_all() {
    for (auto& pair : writers_) {
        pair.second->flush();
    }
}

size_t MultiFileJsonLinesWriter::get_file_count() const {
    return writers_.size();
}

size_t MultiFileJsonLinesWriter::get_total_record_count() const {
    size_t total = 0;
    for (const auto& pair : writers_) {
        total += pair.second->get_record_count();
    }
    return total;
}

} // namespace kraken
