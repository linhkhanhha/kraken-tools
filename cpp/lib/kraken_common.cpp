#include "kraken_common.hpp"
#include <iostream>
#include <cctype>

namespace kraken {

// Get current UTC timestamp
std::string Utils::get_utc_timestamp() {
    auto now = std::chrono::system_clock::now();
    auto time_t = std::chrono::system_clock::to_time_t(now);
    auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(
        now.time_since_epoch()) % 1000;

    std::tm tm = *std::gmtime(&time_t);
    std::ostringstream oss;
    oss << std::put_time(&tm, "%Y-%m-%d %H:%M:%S");
    oss << "." << std::setfill('0') << std::setw(3) << ms.count();
    return oss.str();
}

// Save ticker records to CSV
void Utils::save_to_csv(const std::string& filename,
                       const std::vector<TickerRecord>& records) {
    std::ofstream file(filename);

    if (!file.is_open()) {
        std::cerr << "Error: Could not open file " << filename << std::endl;
        return;
    }

    // Write header
    file << "timestamp,pair,type,bid,bid_qty,ask,ask_qty,last,volume,vwap,low,high,change,change_pct\n";

    // Write data
    for (const auto& record : records) {
        file << record.timestamp << ","
             << record.pair << ","
             << record.type << ","
             << record.bid << ","
             << record.bid_qty << ","
             << record.ask << ","
             << record.ask_qty << ","
             << record.last << ","
             << record.volume << ","
             << record.vwap << ","
             << record.low << ","
             << record.high << ","
             << record.change << ","
             << record.change_pct << "\n";
    }

    file.close();
    std::cout << "\nSaved to " << filename << std::endl;
    std::cout << "Total records: " << records.size() << std::endl;
}

// Print CSV header
void Utils::print_csv_header() {
    std::cout << "timestamp,pair,type,bid,bid_qty,ask,ask_qty,last,volume,vwap,low,high,change,change_pct" << std::endl;
}

// Print a single record
void Utils::print_record(const TickerRecord& record) {
    std::cout << record.timestamp << " | "
              << record.pair << " | "
              << "last: " << record.last << " | "
              << "change: " << std::fixed << std::setprecision(2)
              << record.change_pct << "%" << std::endl;
}

// Simple JSON parser - extract string value
std::string SimpleJsonParser::extract_string(const std::string& json, const std::string& key) {
    std::string search = "\"" + key + "\":\"";
    size_t pos = json.find(search);
    if (pos == std::string::npos) return "";

    pos += search.length();
    size_t end = json.find("\"", pos);
    if (end == std::string::npos) return "";

    return json.substr(pos, end - pos);
}

// Simple JSON parser - extract number value
double SimpleJsonParser::extract_number(const std::string& json, const std::string& key) {
    std::string search = "\"" + key + "\":";
    size_t pos = json.find(search);
    if (pos == std::string::npos) return 0.0;

    pos += search.length();
    size_t end = pos;

    // Skip whitespace
    while (end < json.length() && std::isspace(json[end])) {
        end++;
    }

    pos = end;

    // Find end of number
    while (end < json.length() &&
           (std::isdigit(json[end]) || json[end] == '.' || json[end] == '-' ||
            json[end] == 'e' || json[end] == 'E' || json[end] == '+')) {
        end++;
    }

    if (end == pos) return 0.0;

    std::string num_str = json.substr(pos, end - pos);
    try {
        return std::stod(num_str);
    } catch (...) {
        return 0.0;
    }
}

// Check if JSON contains key
bool SimpleJsonParser::contains(const std::string& json, const std::string& key) {
    std::string search = "\"" + key + "\"";
    return json.find(search) != std::string::npos;
}

} // namespace kraken
