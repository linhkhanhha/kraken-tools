#ifndef KRAKEN_COMMON_HPP
#define KRAKEN_COMMON_HPP

#include <string>
#include <vector>
#include <fstream>
#include <chrono>
#include <iomanip>
#include <sstream>

namespace kraken {

// Ticker record structure - matches Kraken WebSocket v2 ticker data
struct TickerRecord {
    std::string timestamp;
    std::string pair;
    std::string type;       // "snapshot" or "update"
    double bid;
    double bid_qty;
    double ask;
    double ask_qty;
    double last;
    double volume;
    double vwap;
    double low;
    double high;
    double change;
    double change_pct;
};

// Common utility functions
class Utils {
public:
    /**
     * Get current UTC timestamp in format: YYYY-MM-DD HH:MM:SS.mmm
     */
    static std::string get_utc_timestamp();

    /**
     * Save ticker records to CSV file
     * @param filename Output CSV filename
     * @param records Vector of ticker records to save
     */
    static void save_to_csv(const std::string& filename,
                           const std::vector<TickerRecord>& records);

    /**
     * Print CSV header to console
     */
    static void print_csv_header();

    /**
     * Print a single ticker record to console
     */
    static void print_record(const TickerRecord& record);
};

// Simple JSON parsing utilities (for standalone version without nlohmann/json)
class SimpleJsonParser {
public:
    /**
     * Extract string value from JSON
     * @param json JSON string to parse
     * @param key Key to look for
     * @return Extracted string value or empty string if not found
     */
    static std::string extract_string(const std::string& json, const std::string& key);

    /**
     * Extract numeric value from JSON
     * @param json JSON string to parse
     * @param key Key to look for
     * @return Extracted numeric value or 0.0 if not found
     */
    static double extract_number(const std::string& json, const std::string& key);

    /**
     * Check if JSON contains a key
     */
    static bool contains(const std::string& json, const std::string& key);
};

} // namespace kraken

#endif // KRAKEN_COMMON_HPP
