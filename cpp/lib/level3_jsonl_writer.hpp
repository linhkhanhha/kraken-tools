/**
 * JSON Lines Writer for Level 3 Order Book Data
 *
 * Writes Level3Record data to .jsonl (JSON Lines) format
 * One JSON object per line, suitable for streaming order-level data
 */

#ifndef LEVEL3_JSONL_WRITER_HPP
#define LEVEL3_JSONL_WRITER_HPP

#include "level3_common.hpp"
#include <fstream>
#include <string>
#include <sstream>
#include <iomanip>
#include <map>

namespace kraken {

/**
 * JSON Lines Writer for Level 3 orders
 */
class Level3JsonLinesWriter {
public:
    /**
     * Constructor
     * @param filename Output filename
     * @param append Append to existing file (default: false)
     */
    Level3JsonLinesWriter(const std::string& filename, bool append = false);

    /**
     * Destructor - closes file
     */
    ~Level3JsonLinesWriter();

    /**
     * Write Level 3 order book record to file
     */
    bool write_record(const Level3Record& record);

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
     * Convert Level3Record to JSON string
     */
    std::string record_to_json(const Level3Record& record) const;

    /**
     * Escape JSON string
     */
    std::string escape_json_string(const std::string& str) const;

    /**
     * Format orders array to JSON
     */
    std::string orders_to_json(const std::vector<Level3Order>& orders) const;

    /**
     * Format single order to JSON
     */
    std::string order_to_json(const Level3Order& order) const;
};

/**
 * Multi-file JSON Lines Writer for Level 3
 * Manages separate files per symbol
 */
class MultiFileLevel3JsonLinesWriter {
public:
    /**
     * Constructor
     * @param base_filename Base filename (will be appended with symbol)
     */
    MultiFileLevel3JsonLinesWriter(const std::string& base_filename);

    /**
     * Destructor - closes all files
     */
    ~MultiFileLevel3JsonLinesWriter();

    /**
     * Write record to appropriate file based on symbol
     */
    bool write_record(const Level3Record& record);

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
    std::map<std::string, Level3JsonLinesWriter*> writers_;

    /**
     * Get or create writer for symbol
     */
    Level3JsonLinesWriter* get_writer(const std::string& symbol);

    /**
     * Create filename for symbol
     */
    std::string create_filename(const std::string& symbol) const;

    /**
     * Sanitize symbol for filename
     */
    std::string sanitize_symbol(const std::string& symbol) const;
};

} // namespace kraken

#endif // LEVEL3_JSONL_WRITER_HPP
