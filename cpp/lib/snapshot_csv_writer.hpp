/**
 * Snapshot CSV Writer
 *
 * Writes order book snapshot metrics to CSV format with adaptive precision.
 */

#ifndef SNAPSHOT_CSV_WRITER_HPP
#define SNAPSHOT_CSV_WRITER_HPP

#include <string>
#include <fstream>
#include <map>
#include "orderbook_state.hpp"

namespace kraken {

/**
 * CSV writer for snapshot metrics
 */
class SnapshotCSVWriter {
public:
    /**
     * Constructor
     * @param filename Output CSV filename
     * @param append Append to existing file (default: false)
     */
    SnapshotCSVWriter(const std::string& filename, bool append = false);

    /**
     * Destructor - closes file
     */
    ~SnapshotCSVWriter();

    /**
     * Write snapshot metrics to CSV
     */
    bool write_snapshot(const SnapshotMetrics& metrics);

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
class MultiFileSnapshotCSVWriter {
public:
    /**
     * Constructor
     * @param base_filename Base filename (will be appended with symbol)
     */
    MultiFileSnapshotCSVWriter(const std::string& base_filename);

    /**
     * Destructor - closes all files
     */
    ~MultiFileSnapshotCSVWriter();

    /**
     * Write snapshot to appropriate file based on symbol
     */
    bool write_snapshot(const SnapshotMetrics& metrics);

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
    std::map<std::string, SnapshotCSVWriter*> writers_;

    /**
     * Get or create writer for symbol
     */
    SnapshotCSVWriter* get_writer(const std::string& symbol);

    /**
     * Create filename for symbol
     * E.g., "snapshots_BTC_USD.csv"
     */
    std::string create_filename(const std::string& symbol) const;

    /**
     * Sanitize symbol for filename (replace / with _)
     */
    std::string sanitize_symbol(const std::string& symbol) const;
};

} // namespace kraken

#endif // SNAPSHOT_CSV_WRITER_HPP
