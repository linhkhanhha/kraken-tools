/**
 * Level 3 Snapshot CSV Writer
 *
 * Writes Level 3 order book snapshot metrics to CSV format with adaptive precision.
 */

#ifndef LEVEL3_CSV_WRITER_HPP
#define LEVEL3_CSV_WRITER_HPP

#include <string>
#include <fstream>
#include <map>
#include "level3_state.hpp"

namespace kraken {

/**
 * CSV writer for Level 3 snapshot metrics
 */
class Level3CSVWriter {
public:
    /**
     * Constructor
     * @param filename Output CSV filename
     * @param append Append to existing file (default: false)
     */
    Level3CSVWriter(const std::string& filename, bool append = false);

    /**
     * Destructor - closes file
     */
    ~Level3CSVWriter();

    /**
     * Write snapshot metrics to CSV
     */
    bool write_snapshot(const Level3SnapshotMetrics& metrics);

    /**
     * Flush buffered data to disk
     */
    void flush();

    /**
     * Check if file is open and writable
     */
    bool is_open() const;

    /**
     * Get number of snapshots written
     */
    size_t get_snapshot_count() const;

private:
    std::ofstream file_;
    std::string filename_;
    size_t snapshot_count_;
    bool header_written_;

    /**
     * Write CSV header
     */
    void write_header();

    /**
     * Format double with adaptive precision (no trailing zeros)
     */
    std::string format_double(double value) const;
};

/**
 * Multi-file CSV writer - separate file per symbol
 */
class MultiFileLevel3CSVWriter {
public:
    /**
     * Constructor
     * @param base_filename Base filename (will be appended with symbol)
     */
    MultiFileLevel3CSVWriter(const std::string& base_filename);

    /**
     * Destructor - closes all files
     */
    ~MultiFileLevel3CSVWriter();

    /**
     * Write snapshot to appropriate file based on symbol
     */
    bool write_snapshot(const Level3SnapshotMetrics& metrics);

    /**
     * Flush all files
     */
    void flush_all();

    /**
     * Get number of files open
     */
    size_t get_file_count() const;

    /**
     * Get total snapshots written across all files
     */
    size_t get_total_snapshot_count() const;

private:
    std::string base_filename_;
    std::map<std::string, Level3CSVWriter*> writers_;

    /**
     * Get or create writer for symbol
     */
    Level3CSVWriter* get_writer(const std::string& symbol);

    /**
     * Create filename for symbol
     * E.g., "level3_snapshots_BTC_USD.csv"
     */
    std::string create_filename(const std::string& symbol) const;

    /**
     * Sanitize symbol for filename (replace / with _)
     */
    std::string sanitize_symbol(const std::string& symbol) const;
};

} // namespace kraken

#endif // LEVEL3_CSV_WRITER_HPP
