/**
 * CLI Utilities Library Implementation
 */

#include "cli_utils.hpp"
#include <fstream>
#include <sstream>
#include <algorithm>
#include <iostream>
#include <iomanip>
#include <cstring>
#include <sys/stat.h>

namespace cli {

// ============================================================================
// StringUtils Implementation
// ============================================================================

std::string StringUtils::trim(const std::string& str) {
    size_t first = str.find_first_not_of(" \t\r\n");
    if (first == std::string::npos) return "";
    size_t last = str.find_last_not_of(" \t\r\n");
    return str.substr(first, last - first + 1);
}

std::vector<std::string> StringUtils::split(const std::string& str, char delimiter) {
    std::vector<std::string> tokens;
    std::stringstream ss(str);
    std::string token;

    while (std::getline(ss, token, delimiter)) {
        tokens.push_back(token);
    }

    return tokens;
}

std::string StringUtils::join(const std::vector<std::string>& strings, const std::string& delimiter) {
    if (strings.empty()) return "";

    std::string result = strings[0];
    for (size_t i = 1; i < strings.size(); i++) {
        result += delimiter + strings[i];
    }
    return result;
}

bool StringUtils::starts_with(const std::string& str, const std::string& prefix) {
    if (prefix.length() > str.length()) return false;
    return str.compare(0, prefix.length(), prefix) == 0;
}

bool StringUtils::ends_with(const std::string& str, const std::string& suffix) {
    if (suffix.length() > str.length()) return false;
    return str.compare(str.length() - suffix.length(), suffix.length(), suffix) == 0;
}

std::string StringUtils::to_lower(const std::string& str) {
    std::string result = str;
    std::transform(result.begin(), result.end(), result.begin(), ::tolower);
    return result;
}

std::string StringUtils::to_upper(const std::string& str) {
    std::string result = str;
    std::transform(result.begin(), result.end(), result.begin(), ::toupper);
    return result;
}

// ============================================================================
// CSVParser Implementation
// ============================================================================

std::vector<std::string> CSVParser::parse_column(
    const std::string& filepath,
    const std::string& column_name,
    int limit
) {
    std::vector<std::string> values;
    std::ifstream file(filepath);

    if (!file.is_open()) {
        std::cerr << "Error: Cannot open file: " << filepath << std::endl;
        return values;
    }

    // Read header line
    std::string header_line;
    if (!std::getline(file, header_line)) {
        std::cerr << "Error: Empty CSV file" << std::endl;
        return values;
    }

    // Find column index
    auto headers = StringUtils::split(header_line, ',');
    int column_index = -1;

    for (size_t i = 0; i < headers.size(); i++) {
        if (StringUtils::trim(headers[i]) == column_name) {
            column_index = static_cast<int>(i);
            break;
        }
    }

    if (column_index == -1) {
        std::cerr << "Error: Column '" << column_name << "' not found in CSV" << std::endl;
        std::cerr << "Available columns: ";
        for (size_t i = 0; i < headers.size(); i++) {
            if (i > 0) std::cerr << ", ";
            std::cerr << StringUtils::trim(headers[i]);
        }
        std::cerr << std::endl;
        return values;
    }

    // Read data lines
    std::string line;
    int count = 0;
    while (std::getline(file, line) && (limit < 0 || count < limit)) {
        auto fields = StringUtils::split(line, ',');

        if (column_index < static_cast<int>(fields.size())) {
            std::string value = StringUtils::trim(fields[column_index]);
            if (!value.empty()) {
                values.push_back(value);
                count++;
            }
        }
    }

    file.close();
    return values;
}

std::vector<std::string> CSVParser::get_columns(const std::string& filepath) {
    std::vector<std::string> columns;
    std::ifstream file(filepath);

    if (!file.is_open()) {
        return columns;
    }

    std::string header_line;
    if (std::getline(file, header_line)) {
        auto headers = StringUtils::split(header_line, ',');
        for (const auto& header : headers) {
            columns.push_back(StringUtils::trim(header));
        }
    }

    file.close();
    return columns;
}

std::map<std::string, std::vector<std::string>> CSVParser::read_csv(
    const std::string& filepath,
    int limit
) {
    std::map<std::string, std::vector<std::string>> data;
    std::ifstream file(filepath);

    if (!file.is_open()) {
        return data;
    }

    // Read header
    std::string header_line;
    if (!std::getline(file, header_line)) {
        return data;
    }

    auto headers = StringUtils::split(header_line, ',');
    for (auto& header : headers) {
        header = StringUtils::trim(header);
        data[header] = std::vector<std::string>();
    }

    // Read data
    std::string line;
    int count = 0;
    while (std::getline(file, line) && (limit < 0 || count < limit)) {
        auto fields = StringUtils::split(line, ',');

        for (size_t i = 0; i < fields.size() && i < headers.size(); i++) {
            data[headers[i]].push_back(StringUtils::trim(fields[i]));
        }
        count++;
    }

    file.close();
    return data;
}

int CSVParser::count_rows(const std::string& filepath) {
    std::ifstream file(filepath);
    if (!file.is_open()) return 0;

    int count = -1; // Don't count header
    std::string line;
    while (std::getline(file, line)) {
        count++;
    }

    file.close();
    return count;
}

// ============================================================================
// TextFileParser Implementation
// ============================================================================

std::vector<std::string> TextFileParser::parse_lines(const std::string& filepath, int limit) {
    std::vector<std::string> lines;
    std::ifstream file(filepath);

    if (!file.is_open()) {
        std::cerr << "Error: Cannot open file: " << filepath << std::endl;
        return lines;
    }

    std::string line;
    int count = 0;
    while (std::getline(file, line) && (limit < 0 || count < limit)) {
        std::string trimmed = StringUtils::trim(line);
        // Skip empty lines and comments
        if (!trimmed.empty() && trimmed[0] != '#') {
            lines.push_back(trimmed);
            count++;
        }
    }

    file.close();
    return lines;
}

int TextFileParser::count_lines(const std::string& filepath) {
    std::ifstream file(filepath);
    if (!file.is_open()) return 0;

    int count = 0;
    std::string line;
    while (std::getline(file, line)) {
        std::string trimmed = StringUtils::trim(line);
        if (!trimmed.empty() && trimmed[0] != '#') {
            count++;
        }
    }

    file.close();
    return count;
}

// ============================================================================
// ListParser Implementation
// ============================================================================

std::vector<std::string> ListParser::parse(const std::string& input) {
    return parse(input, ',');
}

std::vector<std::string> ListParser::parse(const std::string& input, char delimiter) {
    std::vector<std::string> values;
    auto tokens = StringUtils::split(input, delimiter);

    for (const auto& token : tokens) {
        std::string trimmed = StringUtils::trim(token);
        if (!trimmed.empty()) {
            values.push_back(trimmed);
        }
    }

    return values;
}

// ============================================================================
// InputParser Implementation
// ============================================================================

bool InputParser::is_file_format(const std::string& input) {
    // Check if input looks like a file path
    // More robust heuristic:
    // 1. Has file extension (.csv, .txt, etc.) OR
    // 2. Starts with '/' (absolute path) OR
    // 3. Starts with './' or '../' (relative path) OR
    // 4. Contains ':' AND has a file extension (CSV with column spec)

    bool has_extension = (input.find(".csv") != std::string::npos) ||
                         (input.find(".txt") != std::string::npos);

    bool starts_with_path = (input[0] == '/') ||
                           (input.size() >= 2 && input.substr(0, 2) == "./") ||
                           (input.size() >= 3 && input.substr(0, 3) == "../");

    bool csv_with_column = (input.find(':') != std::string::npos) && has_extension;

    return has_extension || starts_with_path || csv_with_column;
}

bool InputParser::is_text_file(const std::string& filepath) {
    return StringUtils::ends_with(StringUtils::to_lower(filepath), ".txt");
}

InputParser::ParseResult InputParser::parse_text_file(const std::string& input) {
    ParseResult result;
    result.type = InputType::TEXT_FILE;
    result.success = false;
    result.limit = -1;

    size_t colon_pos = input.find(':');

    if (colon_pos == std::string::npos) {
        // Format: file.txt (all lines)
        result.filepath = input;
    } else {
        // Format: file.txt:limit
        result.filepath = input.substr(0, colon_pos);
        std::string limit_str = input.substr(colon_pos + 1);

        try {
            result.limit = std::stoi(limit_str);
        } catch (...) {
            result.error_message = "Invalid limit: " + limit_str;
            return result;
        }
    }

    // Extract values from text file
    result.values = TextFileParser::parse_lines(result.filepath, result.limit);

    if (result.values.empty()) {
        result.error_message = "No values extracted from text file";
        return result;
    }

    result.success = true;
    return result;
}

InputParser::ParseResult InputParser::parse_csv_format(const std::string& input) {
    ParseResult result;
    result.type = InputType::CSV_FILE;
    result.success = false;
    result.limit = -1;

    size_t first_colon = input.find(':');
    if (first_colon == std::string::npos) {
        result.error_message = "Invalid CSV format - missing column specification";
        return result;
    }

    result.filepath = input.substr(0, first_colon);
    std::string remainder = input.substr(first_colon + 1);

    size_t second_colon = remainder.find(':');
    if (second_colon == std::string::npos) {
        // Format: file.csv:column
        result.column_name = remainder;
    } else {
        // Format: file.csv:column:limit
        result.column_name = remainder.substr(0, second_colon);
        std::string limit_str = remainder.substr(second_colon + 1);

        try {
            result.limit = std::stoi(limit_str);
        } catch (...) {
            result.error_message = "Invalid limit: " + limit_str;
            return result;
        }
    }

    // Extract values from CSV
    result.values = CSVParser::parse_column(result.filepath, result.column_name, result.limit);

    if (result.values.empty()) {
        result.error_message = "No values extracted from CSV";
        return result;
    }

    result.success = true;
    return result;
}

InputParser::ParseResult InputParser::parse_direct_list(const std::string& input, char delimiter) {
    ParseResult result;
    result.type = InputType::DIRECT_LIST;
    result.success = false;

    result.values = ListParser::parse(input, delimiter);

    if (result.values.empty()) {
        result.error_message = "No values found in list";
        return result;
    }

    result.success = true;
    return result;
}

InputParser::ParseResult InputParser::parse(const std::string& input) {
    return parse(input, ',');
}

InputParser::ParseResult InputParser::parse(const std::string& input, char list_delimiter) {
    if (input.empty()) {
        ParseResult result;
        result.type = InputType::UNKNOWN;
        result.success = false;
        result.error_message = "Empty input";
        return result;
    }

    // Check if it's a file format
    if (is_file_format(input)) {
        // Extract filepath (before any colon)
        size_t colon_pos = input.find(':');
        std::string filepath = (colon_pos == std::string::npos) ? input : input.substr(0, colon_pos);

        // Check if it's a text file
        if (is_text_file(filepath)) {
            return parse_text_file(input);
        } else {
            // Assume CSV file (must have column specification with :)
            return parse_csv_format(input);
        }
    } else {
        // Direct list
        return parse_direct_list(input, list_delimiter);
    }
}

// ============================================================================
// ArgumentParser Implementation
// ============================================================================

ArgumentParser::ArgumentParser(const std::string& program_name, const std::string& description)
    : program_name_(program_name), description_(description) {
}

void ArgumentParser::add_argument(const Argument& arg) {
    arguments_.push_back(arg);
}

const ArgumentParser::Argument* ArgumentParser::find_argument(const std::string& flag) const {
    for (const auto& arg : arguments_) {
        if (arg.short_flag == flag || arg.long_flag == flag) {
            return &arg;
        }
    }
    return nullptr;
}

std::string ArgumentParser::normalize_flag(const std::string& flag) const {
    // Prefer short flag for internal storage, unless it's empty
    const Argument* arg = find_argument(flag);
    if (arg) {
        return arg->short_flag.empty() ? arg->long_flag : arg->short_flag;
    }
    return flag;
}

bool ArgumentParser::parse(int argc, char* argv[]) {
    errors_.clear();
    values_.clear();

    for (int i = 1; i < argc; i++) {
        std::string arg = argv[i];

        // Check for help flags
        if (arg == "-h" || arg == "--help") {
            print_help();
            return false;
        }

        const Argument* arg_def = find_argument(arg);

        if (!arg_def) {
            errors_.push_back("Unknown argument: " + arg);
            continue;
        }

        std::string key = normalize_flag(arg);

        if (arg_def->has_value) {
            if (i + 1 < argc) {
                values_[key] = argv[++i];
            } else {
                errors_.push_back("Argument " + arg + " requires a value");
            }
        } else {
            values_[key] = "true";
        }
    }

    // Check required arguments
    for (const auto& arg : arguments_) {
        std::string key = arg.short_flag.empty() ? arg.long_flag : arg.short_flag;
        if (arg.required && values_.find(key) == values_.end()) {
            std::string display = arg.short_flag.empty() ? arg.long_flag : arg.short_flag;
            errors_.push_back("Required argument missing: " + display);
        }
    }

    return errors_.empty();
}

std::string ArgumentParser::get(const std::string& flag) const {
    std::string key = normalize_flag(flag);
    auto it = values_.find(key);

    if (it != values_.end()) {
        return it->second;
    }

    // Return default value
    const Argument* arg = find_argument(flag);
    return arg ? arg->default_value : "";
}

bool ArgumentParser::has(const std::string& flag) const {
    std::string key = normalize_flag(flag);
    return values_.find(key) != values_.end();
}

void ArgumentParser::print_help() const {
    std::cout << "Usage: " << program_name_ << " [options]" << std::endl;
    std::cout << std::endl;
    std::cout << description_ << std::endl;
    std::cout << std::endl;
    std::cout << "Options:" << std::endl;

    for (const auto& arg : arguments_) {
        std::cout << "  " << std::left << std::setw(20);

        std::string flags = arg.short_flag;
        if (!arg.long_flag.empty()) {
            flags += ", " + arg.long_flag;
        }
        if (arg.has_value && !arg.value_name.empty()) {
            flags += " <" + arg.value_name + ">";
        }

        std::cout << flags << arg.description;

        if (arg.required) {
            std::cout << " (required)";
        } else if (!arg.default_value.empty()) {
            std::cout << " (default: " << arg.default_value << ")";
        }

        std::cout << std::endl;
    }
}

void ArgumentParser::print_error(const std::string& error) const {
    std::cerr << "Error: " << error << std::endl;
    std::cerr << std::endl;
    print_help();
}

std::vector<std::string> ArgumentParser::get_errors() const {
    return errors_;
}

// ============================================================================
// Validator Implementation
// ============================================================================

bool Validator::is_valid_file(const std::string& filepath, std::string* error) {
    struct stat buffer;
    if (stat(filepath.c_str(), &buffer) != 0) {
        if (error) *error = "File does not exist: " + filepath;
        return false;
    }

    if (!S_ISREG(buffer.st_mode)) {
        if (error) *error = "Not a regular file: " + filepath;
        return false;
    }

    std::ifstream file(filepath);
    if (!file.good()) {
        if (error) *error = "Cannot read file: " + filepath;
        return false;
    }

    return true;
}

bool Validator::is_valid_directory(const std::string& dirpath, std::string* error) {
    struct stat buffer;
    if (stat(dirpath.c_str(), &buffer) != 0) {
        if (error) *error = "Directory does not exist: " + dirpath;
        return false;
    }

    if (!S_ISDIR(buffer.st_mode)) {
        if (error) *error = "Not a directory: " + dirpath;
        return false;
    }

    return true;
}

bool Validator::is_in_range(int value, int min, int max, std::string* error) {
    if (value < min || value > max) {
        if (error) {
            *error = "Value " + std::to_string(value) + " out of range [" +
                     std::to_string(min) + ", " + std::to_string(max) + "]";
        }
        return false;
    }
    return true;
}

bool Validator::is_not_empty(const std::string& value, std::string* error) {
    if (value.empty()) {
        if (error) *error = "Value cannot be empty";
        return false;
    }
    return true;
}

bool Validator::matches_pattern(const std::string& value, const std::string& pattern, std::string* error) {
    // Basic wildcard matching (* and ?)
    // This is a simplified implementation
    size_t v_pos = 0, p_pos = 0;

    while (v_pos < value.length() && p_pos < pattern.length()) {
        if (pattern[p_pos] == '*') {
            // Skip consecutive asterisks
            while (p_pos < pattern.length() && pattern[p_pos] == '*') {
                p_pos++;
            }

            if (p_pos == pattern.length()) {
                return true; // * at end matches everything
            }

            // Try to match rest of pattern
            while (v_pos < value.length()) {
                if (matches_pattern(value.substr(v_pos), pattern.substr(p_pos), nullptr)) {
                    return true;
                }
                v_pos++;
            }
            return false;
        } else if (pattern[p_pos] == '?' || pattern[p_pos] == value[v_pos]) {
            v_pos++;
            p_pos++;
        } else {
            if (error) *error = "Value does not match pattern: " + pattern;
            return false;
        }
    }

    // Handle remaining asterisks in pattern
    while (p_pos < pattern.length() && pattern[p_pos] == '*') {
        p_pos++;
    }

    bool matches = (v_pos == value.length() && p_pos == pattern.length());
    if (!matches && error) {
        *error = "Value does not match pattern: " + pattern;
    }

    return matches;
}

} // namespace cli
