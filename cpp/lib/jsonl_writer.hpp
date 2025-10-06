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

namespace kraken {

/**
 * JSON Lines Writer
 * Writes order book records to .jsonl format
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
     * Destructor - closes file
     */
    ~JsonLinesWriter();

    /**
     * Write order book record to file
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
     * Get number of records written
     */
    size_t get_record_count() const;

private:
    std::ofstream file_;
    std::string filename_;
    size_t record_count_;

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
};

/**
 * Multi-file JSON Lines Writer
 * Manages separate files per symbol
 */
class MultiFileJsonLinesWriter {
public:
    /**
     * Constructor
     * @param base_filename Base filename (will be appended with symbol)
     */
    MultiFileJsonLinesWriter(const std::string& base_filename);

    /**
     * Destructor - closes all files
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

private:
    std::string base_filename_;
    std::map<std::string, JsonLinesWriter*> writers_;

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
};

} // namespace kraken

#endif // JSONL_WRITER_HPP
