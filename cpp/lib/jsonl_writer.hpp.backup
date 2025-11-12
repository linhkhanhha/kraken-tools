/**
 * JSON Lines Writer for Order Book Data
 *
 * Writes OrderBookRecord data to .jsonl (JSON Lines) format
 * One JSON object per line, suitable for streaming data
 */

#ifndef JSONL_WRITER_HPP
#define JSONL_WRITER_HPP

#include "orderbook_common.hpp"
#include <fstream>
#include <string>
#include <sstream>
#include <iomanip>
#include <map>
#include <vector>
#include <chrono>
#include <ctime>

namespace kraken {

/**
 * Segmentation mode for time-based file splitting
 */
enum class SegmentMode {
    NONE,    // Single file (default)
    HOURLY,  // One file per hour (YYYYMMDD_HH)
    DAILY    // One file per day (YYYYMMDD)
};

/**
 * JSON Lines Writer
 * Writes order book records to .jsonl format with periodic flushing and segmentation
 */
class JsonLinesWriter {
public:
    /**
     * Constructor
     * @param filename Output filename
     * @param append Append to existing file (default: false)
     */
    JsonLinesWriter(const std::string& filename, bool append = false);

    /**
     * Destructor - closes file and flushes remaining data
     */
    ~JsonLinesWriter();

    /**
     * Write order book record (buffered with periodic flush)
     */
    bool write_record(const OrderBookRecord& record);

    /**
     * Flush buffered data to disk
     */
    void flush();

    /**
     * Check if file is open and writable
     */
    bool is_open() const;

    /**
     * Get number of records written (total across all flushes)
     */
    size_t get_record_count() const;

    // ========================================================================
    // Flush Configuration
    // ========================================================================

    /**
     * Set flush interval (time-based trigger)
     * @param interval Flush interval (0 to disable)
     */
    void set_flush_interval(std::chrono::seconds interval);

    /**
     * Set memory threshold (memory-based trigger)
     * @param bytes Memory threshold in bytes (0 to disable)
     */
    void set_memory_threshold(size_t bytes);

    /**
     * Get flush count
     */
    size_t get_flush_count() const;

    /**
     * Get current memory usage (buffered records)
     */
    size_t get_current_memory_usage() const;

    // ========================================================================
    // Segmentation Configuration
    // ========================================================================

    /**
     * Set segmentation mode
     * @param mode NONE, HOURLY, or DAILY
     */
    void set_segment_mode(SegmentMode mode);

    /**
     * Get current segment filename
     */
    std::string get_current_segment_filename() const;

    /**
     * Get segment count
     */
    size_t get_segment_count() const;

private:
    std::ofstream file_;
    std::string filename_;
    size_t record_count_;

    // ========================================================================
    // Buffering and Flush Configuration
    // ========================================================================
    std::vector<OrderBookRecord> record_buffer_;      // Buffered records
    std::chrono::seconds flush_interval_;             // Time-based flush trigger
    size_t memory_threshold_bytes_;                   // Memory-based flush trigger
    std::chrono::steady_clock::time_point last_flush_time_;
    size_t flush_count_;

    // ========================================================================
    // Segmentation Configuration
    // ========================================================================
    SegmentMode segment_mode_;
    std::string base_filename_;                       // Original filename without segment suffix
    std::string current_segment_filename_;            // Current segment filename
    std::string current_segment_key_;                 // Current segment key (YYYYMMDD_HH or YYYYMMDD)
    size_t segment_count_;

    /**
     * Convert OrderBookRecord to JSON string
     */
    std::string record_to_json(const OrderBookRecord& record) const;

    /**
     * Escape JSON string
     */
    std::string escape_json_string(const std::string& str) const;

    /**
     * Format price level array to JSON
     */
    std::string price_levels_to_json(const std::vector<PriceLevel>& levels) const;

    /**
     * Check if flush should be triggered
     */
    bool should_flush() const;

    /**
     * Flush buffered records to file
     */
    void flush_buffer();

    /**
     * Generate segment key based on current time
     * Returns YYYYMMDD_HH for hourly, YYYYMMDD for daily
     */
    std::string generate_segment_key() const;

    /**
     * Check if segment transition is needed
     */
    bool should_transition_segment();

    /**
     * Create new segment file
     */
    void create_new_segment();

    /**
     * Insert segment key into filename
     * E.g., "orderbook.jsonl" -> "orderbook.20251112_10.jsonl"
     */
    std::string insert_segment_key(const std::string& base, const std::string& key) const;
};

/**
 * Multi-file JSON Lines Writer
 * Manages separate files per symbol with periodic flushing and segmentation
 */
class MultiFileJsonLinesWriter {
public:
    /**
     * Constructor
     * @param base_filename Base filename (will be appended with symbol)
     */
    MultiFileJsonLinesWriter(const std::string& base_filename);

    /**
     * Destructor - closes all files and flushes remaining data
     */
    ~MultiFileJsonLinesWriter();

    /**
     * Write record to appropriate file based on symbol
     */
    bool write_record(const OrderBookRecord& record);

    /**
     * Flush all files
     */
    void flush_all();

    /**
     * Get number of files open
     */
    size_t get_file_count() const;

    /**
     * Get total records written across all files
     */
    size_t get_total_record_count() const;

    // ========================================================================
    // Flush Configuration (applies to all writers)
    // ========================================================================

    /**
     * Set flush interval for all writers
     */
    void set_flush_interval(std::chrono::seconds interval);

    /**
     * Set memory threshold for all writers
     */
    void set_memory_threshold(size_t bytes);

    /**
     * Get total flush count across all writers
     */
    size_t get_total_flush_count() const;

    /**
     * Get total memory usage across all writers
     */
    size_t get_total_memory_usage() const;

    // ========================================================================
    // Segmentation Configuration (applies to all writers)
    // ========================================================================

    /**
     * Set segmentation mode for all writers
     */
    void set_segment_mode(SegmentMode mode);

    /**
     * Get total segment count across all writers
     */
    size_t get_total_segment_count() const;

private:
    std::string base_filename_;
    std::map<std::string, JsonLinesWriter*> writers_;

    // Configuration to apply to all new writers
    std::chrono::seconds flush_interval_;
    size_t memory_threshold_bytes_;
    SegmentMode segment_mode_;

    /**
     * Get or create writer for symbol
     */
    JsonLinesWriter* get_writer(const std::string& symbol);

    /**
     * Create filename for symbol
     * E.g., "orderbook_BTC_USD.jsonl"
     */
    std::string create_filename(const std::string& symbol) const;

    /**
     * Sanitize symbol for filename
     * Replace / with _
     */
    std::string sanitize_symbol(const std::string& symbol) const;

    /**
     * Apply configuration to a writer
     */
    void apply_configuration(JsonLinesWriter* writer);
};

} // namespace kraken

#endif // JSONL_WRITER_HPP
