/**
 * JSON Lines Writer for Order Book Data - Implementation (CRTP Refactored Version)
 */

#include "jsonl_writer.hpp"
#include <iostream>

namespace kraken {

// ============================================================================
// JsonLinesWriter Implementation
// ============================================================================

JsonLinesWriter::JsonLinesWriter(const std::string& filename, bool append)
    : FlushSegmentMixin<JsonLinesWriter>(),  // Initialize mixin
      record_count_(0) {

    // Store base filename for segmentation
    set_base_filename(filename);

    // Note: File opens later:
    // - When set_segment_mode() is called (if segmentation enabled)
    // - On first write_record() (if segmentation disabled)
    record_buffer_.reserve(1000);  // Pre-allocate for efficiency
}

JsonLinesWriter::~JsonLinesWriter() {
    // Flush any remaining buffered records
    if (!record_buffer_.empty()) {
        force_flush();
    }

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

bool JsonLinesWriter::write_record(const OrderBookRecord& record) {
    // Open file on first write if not already open (non-segmented mode)
    if (!file_.is_open() && segment_mode_ == SegmentMode::NONE) {
        file_.open(base_filename_, std::ios::out);
        if (!file_.is_open()) {
            std::cerr << "Error: Cannot open file for writing: " << base_filename_ << std::endl;
            return false;
        }
        current_segment_filename_ = base_filename_;
    }

    if (!file_.is_open()) {
        return false;
    }

    // Add record to buffer
    record_buffer_.push_back(record);

    // CRTP: Single call handles everything automatically
    // - Segment transition detection
    // - Flush before segment transition
    // - Regular periodic flush
    // - Statistics tracking
    check_and_flush();

    return true;
}

void JsonLinesWriter::flush() {
    force_flush();
}

// ============================================================================
// CRTP Interface Implementation
// ============================================================================

void JsonLinesWriter::perform_flush() {
    if (!file_.is_open() || record_buffer_.empty()) {
        return;
    }

    // Write all buffered records to file
    for (const auto& record : record_buffer_) {
        std::string json = record_to_json(record);
        file_ << json << std::endl;
        record_count_++;
    }

    // Flush to disk
    file_.flush();

    // Clear buffer
    record_buffer_.clear();
    record_buffer_.reserve(1000);
}

void JsonLinesWriter::perform_segment_transition(const std::string& new_filename) {
    // Close current file
    if (file_.is_open()) {
        file_.close();
    }

    // Open new segment file (use 'out' only to overwrite, not append)
    file_.open(new_filename, std::ios::out);

    if (!file_.is_open()) {
        std::cerr << "Error: Cannot open segment file: " << new_filename << std::endl;
    }
}

void JsonLinesWriter::on_segment_mode_set() {
    // Create first segment file when segmentation is enabled
    perform_segment_transition(current_segment_filename_);
}

// ============================================================================
// JSON Serialization
// ============================================================================

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

// ============================================================================
// MultiFileJsonLinesWriter Implementation
// ============================================================================

MultiFileJsonLinesWriter::MultiFileJsonLinesWriter(const std::string& base_filename)
    : base_filename_(base_filename),
      flush_interval_(30),                           // Default: 30 seconds
      memory_threshold_bytes_(10 * 1024 * 1024),    // Default: 10 MB
      segment_mode_(SegmentMode::NONE) {
}

MultiFileJsonLinesWriter::~MultiFileJsonLinesWriter() {
    // Flush all writers before destruction
    flush_all();

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

    // Apply configuration to new writer
    // (This may open the file if segmentation is enabled)
    apply_configuration(writer);

    // Note: For non-segmented mode, file will open on first write
    // For segmented mode, file should be open after apply_configuration
    // We don't check is_open() here since lazy opening is intentional

    writers_[symbol] = writer;
    return writer;
}

void MultiFileJsonLinesWriter::apply_configuration(JsonLinesWriter* writer) {
    if (!writer) return;

    writer->set_flush_interval(flush_interval_);
    writer->set_memory_threshold(memory_threshold_bytes_);
    writer->set_segment_mode(segment_mode_);
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

// ========================================================================
// Flush Configuration
// ========================================================================

void MultiFileJsonLinesWriter::set_flush_interval(std::chrono::seconds interval) {
    flush_interval_ = interval;
    // Apply to all existing writers
    for (auto& pair : writers_) {
        pair.second->set_flush_interval(interval);
    }
}

void MultiFileJsonLinesWriter::set_memory_threshold(size_t bytes) {
    memory_threshold_bytes_ = bytes;
    // Apply to all existing writers
    for (auto& pair : writers_) {
        pair.second->set_memory_threshold(bytes);
    }
}

size_t MultiFileJsonLinesWriter::get_total_flush_count() const {
    size_t total = 0;
    for (const auto& pair : writers_) {
        total += pair.second->get_flush_count();
    }
    return total;
}

size_t MultiFileJsonLinesWriter::get_total_memory_usage() const {
    size_t total = 0;
    for (const auto& pair : writers_) {
        total += pair.second->get_current_memory_usage();
    }
    return total;
}

// ========================================================================
// Segmentation Configuration
// ========================================================================

void MultiFileJsonLinesWriter::set_segment_mode(SegmentMode mode) {
    segment_mode_ = mode;
    // Apply to all existing writers
    for (auto& pair : writers_) {
        pair.second->set_segment_mode(mode);
    }
}

size_t MultiFileJsonLinesWriter::get_total_segment_count() const {
    size_t total = 0;
    for (const auto& pair : writers_) {
        total += pair.second->get_segment_count();
    }
    return total;
}

} // namespace kraken
