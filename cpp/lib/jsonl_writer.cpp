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
    : filename_(filename),
      record_count_(0),
      flush_interval_(30),                           // Default: 30 seconds
      memory_threshold_bytes_(10 * 1024 * 1024),    // Default: 10 MB
      flush_count_(0),
      segment_mode_(SegmentMode::NONE),
      base_filename_(filename),
      segment_count_(0) {

    last_flush_time_ = std::chrono::steady_clock::now();
    record_buffer_.reserve(1000);  // Pre-allocate for efficiency

    // Note: Don't open file here - wait for configuration to be set
    // File will be opened either:
    // 1. When set_segment_mode() is called (if segmentation enabled)
    // 2. On first write_record() (if segmentation disabled)
    current_segment_filename_ = filename;
}

JsonLinesWriter::~JsonLinesWriter() {
    // Flush any remaining buffered records
    if (!record_buffer_.empty()) {
        flush_buffer();
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
    // Open file on first write if not already open (for non-segmented mode)
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

    // Check for segment transition
    if (should_transition_segment()) {
        // Flush current buffer before transitioning
        if (!record_buffer_.empty()) {
            flush_buffer();
        }
        create_new_segment();
    }

    // Add record to buffer
    record_buffer_.push_back(record);

    // Check if flush is needed
    if (should_flush()) {
        flush_buffer();
    }

    return true;
}

void JsonLinesWriter::flush() {
    if (!record_buffer_.empty()) {
        flush_buffer();
    }
}

// ========================================================================
// Flush Configuration
// ========================================================================

void JsonLinesWriter::set_flush_interval(std::chrono::seconds interval) {
    flush_interval_ = interval;
}

void JsonLinesWriter::set_memory_threshold(size_t bytes) {
    memory_threshold_bytes_ = bytes;
}

size_t JsonLinesWriter::get_flush_count() const {
    return flush_count_;
}

size_t JsonLinesWriter::get_current_memory_usage() const {
    return record_buffer_.size() * sizeof(OrderBookRecord);
}

// ========================================================================
// Segmentation Configuration
// ========================================================================

void JsonLinesWriter::set_segment_mode(SegmentMode mode) {
    segment_mode_ = mode;
    if (mode != SegmentMode::NONE) {
        // Close existing file if open
        if (file_.is_open()) {
            file_.close();
        }

        // Create first segment
        current_segment_key_ = generate_segment_key();
        current_segment_filename_ = insert_segment_key(base_filename_, current_segment_key_);

        // Open first segment file
        file_.open(current_segment_filename_, std::ios::out | std::ios::app);

        if (!file_.is_open()) {
            std::cerr << "Error: Cannot open segment file: " << current_segment_filename_ << std::endl;
            return;
        }

        segment_count_ = 1;
        std::cout << "[SEGMENT] Starting new file: " << current_segment_filename_ << std::endl;
    }
}

std::string JsonLinesWriter::get_current_segment_filename() const {
    return current_segment_filename_;
}

size_t JsonLinesWriter::get_segment_count() const {
    return segment_count_;
}

// ========================================================================
// Private Helper Methods
// ========================================================================

bool JsonLinesWriter::should_flush() const {
    if (record_buffer_.empty()) {
        return false;
    }

    // Time-based trigger
    bool time_exceeded = false;
    if (flush_interval_.count() > 0) {
        auto now = std::chrono::steady_clock::now();
        auto elapsed = std::chrono::duration_cast<std::chrono::seconds>(
            now - last_flush_time_);
        time_exceeded = (elapsed >= flush_interval_);
    }

    // Memory-based trigger
    bool memory_exceeded = false;
    if (memory_threshold_bytes_ > 0) {
        size_t memory_bytes = record_buffer_.size() * sizeof(OrderBookRecord);
        memory_exceeded = (memory_bytes >= memory_threshold_bytes_);
    }

    // OR logic: flush if either condition is met
    return time_exceeded || memory_exceeded;
}

void JsonLinesWriter::flush_buffer() {
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

    // Update statistics
    flush_count_++;
    last_flush_time_ = std::chrono::steady_clock::now();

    // Log flush (quiet mode after 3 flushes)
    if (flush_count_ <= 3) {
        std::cout << "[FLUSH] Wrote records to " << current_segment_filename_ << std::endl;
    }
}

std::string JsonLinesWriter::generate_segment_key() const {
    auto now = std::time(nullptr);
    auto tm = *std::gmtime(&now);

    char buffer[32];
    if (segment_mode_ == SegmentMode::HOURLY) {
        // Format: YYYYMMDD_HH
        std::strftime(buffer, sizeof(buffer), "%Y%m%d_%H", &tm);
    } else if (segment_mode_ == SegmentMode::DAILY) {
        // Format: YYYYMMDD
        std::strftime(buffer, sizeof(buffer), "%Y%m%d", &tm);
    } else {
        return "";
    }

    return std::string(buffer);
}

bool JsonLinesWriter::should_transition_segment() {
    if (segment_mode_ == SegmentMode::NONE) {
        return false;
    }

    std::string new_key = generate_segment_key();
    return new_key != current_segment_key_;
}

void JsonLinesWriter::create_new_segment() {
    // Close current file
    if (file_.is_open()) {
        file_.close();
    }

    // Generate new segment key
    current_segment_key_ = generate_segment_key();

    // Create new filename
    current_segment_filename_ = insert_segment_key(base_filename_, current_segment_key_);

    // Open new file
    file_.open(current_segment_filename_, std::ios::out | std::ios::app);

    if (!file_.is_open()) {
        std::cerr << "Error: Cannot open segment file: " << current_segment_filename_ << std::endl;
        return;
    }

    // Increment segment count
    segment_count_++;

    // Log segment transition
    std::cout << "[SEGMENT] Starting new file: " << current_segment_filename_ << std::endl;
}

std::string JsonLinesWriter::insert_segment_key(const std::string& base, const std::string& key) const {
    // Find the extension (.jsonl)
    size_t ext_pos = base.rfind(".jsonl");
    if (ext_pos == std::string::npos) {
        // No .jsonl extension, just append
        return base + "." + key + ".jsonl";
    }

    // Insert segment key before extension
    std::string result = base.substr(0, ext_pos);
    result += "." + key + ".jsonl";
    return result;
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
