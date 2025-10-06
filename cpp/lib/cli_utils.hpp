/**
 * CLI Utilities Library
 *
 * Reusable command-line interface utilities for building user-friendly tools.
 * Includes:
 * - Command-line argument parsing
 * - CSV file parsing and column extraction
 * - Input validation and error handling
 * - Help message formatting
 */

#ifndef CLI_UTILS_HPP
#define CLI_UTILS_HPP

#include <string>
#include <vector>
#include <map>
#include <functional>

namespace cli {

/**
 * String manipulation utilities
 */
class StringUtils {
public:
    /**
     * Trim whitespace from both ends of string
     */
    static std::string trim(const std::string& str);

    /**
     * Split string by delimiter
     */
    static std::vector<std::string> split(const std::string& str, char delimiter);

    /**
     * Join strings with delimiter
     */
    static std::string join(const std::vector<std::string>& strings, const std::string& delimiter);

    /**
     * Check if string starts with prefix
     */
    static bool starts_with(const std::string& str, const std::string& prefix);

    /**
     * Check if string ends with suffix
     */
    static bool ends_with(const std::string& str, const std::string& suffix);

    /**
     * Convert string to lowercase
     */
    static std::string to_lower(const std::string& str);

    /**
     * Convert string to uppercase
     */
    static std::string to_upper(const std::string& str);
};

/**
 * CSV file parser
 */
class CSVParser {
public:
    /**
     * Parse CSV file and extract values from specified column
     * @param filepath Path to CSV file
     * @param column_name Name of column to extract
     * @param limit Maximum number of rows to read (-1 for all)
     * @return Vector of values from the specified column
     */
    static std::vector<std::string> parse_column(
        const std::string& filepath,
        const std::string& column_name,
        int limit = -1
    );

    /**
     * Get list of column names from CSV file
     * @param filepath Path to CSV file
     * @return Vector of column names
     */
    static std::vector<std::string> get_columns(const std::string& filepath);

    /**
     * Read entire CSV file into map of column_name -> values
     * @param filepath Path to CSV file
     * @param limit Maximum number of rows to read (-1 for all)
     * @return Map of column names to value vectors
     */
    static std::map<std::string, std::vector<std::string>> read_csv(
        const std::string& filepath,
        int limit = -1
    );

    /**
     * Count rows in CSV file (excluding header)
     * @param filepath Path to CSV file
     * @return Number of data rows
     */
    static int count_rows(const std::string& filepath);
};

/**
 * Text file parser (plain text, one value per line)
 */
class TextFileParser {
public:
    /**
     * Parse text file and read values (one per line, no header)
     * @param filepath Path to text file
     * @param limit Maximum number of lines to read (-1 for all)
     * @return Vector of trimmed non-empty lines
     */
    static std::vector<std::string> parse_lines(
        const std::string& filepath,
        int limit = -1
    );

    /**
     * Count non-empty lines in text file
     * @param filepath Path to text file
     * @return Number of non-empty lines
     */
    static int count_lines(const std::string& filepath);
};

/**
 * List input parser (comma-separated values)
 */
class ListParser {
public:
    /**
     * Parse comma-separated list
     * @param input Input string with comma-separated values
     * @return Vector of trimmed values
     */
    static std::vector<std::string> parse(const std::string& input);

    /**
     * Parse comma-separated list with custom delimiter
     * @param input Input string
     * @param delimiter Custom delimiter
     * @return Vector of trimmed values
     */
    static std::vector<std::string> parse(const std::string& input, char delimiter);
};

/**
 * Flexible input specification parser
 * Supports multiple formats:
 * - Direct list: "VALUE1,VALUE2,VALUE3"
 * - CSV file: "/path/to/file.csv:column_name" or "/path/to/file.csv:column_name:10"
 * - Text file: "/path/to/file.txt" or "/path/to/file.txt:10"
 */
class InputParser {
public:
    enum class InputType {
        DIRECT_LIST,    // Comma-separated values
        CSV_FILE,       // CSV file with column specification
        TEXT_FILE,      // Plain text file (one value per line)
        UNKNOWN
    };

    struct ParseResult {
        InputType type;
        std::vector<std::string> values;
        std::string error_message;
        bool success;

        // For CSV input
        std::string filepath;
        std::string column_name;
        int limit;
    };

    /**
     * Parse input specification and extract values
     * @param input Input string in any supported format
     * @return ParseResult with extracted values and metadata
     */
    static ParseResult parse(const std::string& input);

    /**
     * Parse input specification with custom list delimiter
     * @param input Input string
     * @param list_delimiter Delimiter for direct list format
     * @return ParseResult with extracted values
     */
    static ParseResult parse(const std::string& input, char list_delimiter);

private:
    static bool is_file_format(const std::string& input);
    static bool is_text_file(const std::string& filepath);
    static ParseResult parse_text_file(const std::string& input);
    static ParseResult parse_csv_format(const std::string& input);
    static ParseResult parse_direct_list(const std::string& input, char delimiter);
};

/**
 * Command-line argument parser
 */
class ArgumentParser {
public:
    struct Argument {
        std::string short_flag;      // e.g., "-p"
        std::string long_flag;       // e.g., "--pairs"
        std::string description;
        bool required;
        bool has_value;              // true if argument takes a value
        std::string default_value;
        std::string value_name;      // e.g., "FILE" for display in help
    };

    ArgumentParser(const std::string& program_name, const std::string& description);

    /**
     * Add argument definition
     */
    void add_argument(const Argument& arg);

    /**
     * Parse command-line arguments
     * @param argc Argument count from main()
     * @param argv Argument array from main()
     * @return true if parsing succeeded
     */
    bool parse(int argc, char* argv[]);

    /**
     * Get value of argument by flag
     * @param flag Short or long flag (e.g., "-p" or "--pairs")
     * @return Argument value or empty string if not set
     */
    std::string get(const std::string& flag) const;

    /**
     * Check if argument was provided
     */
    bool has(const std::string& flag) const;

    /**
     * Print help message
     */
    void print_help() const;

    /**
     * Print error message and help
     */
    void print_error(const std::string& error) const;

    /**
     * Get all errors from parsing
     */
    std::vector<std::string> get_errors() const;

private:
    std::string program_name_;
    std::string description_;
    std::vector<Argument> arguments_;
    std::map<std::string, std::string> values_;
    std::vector<std::string> errors_;

    const Argument* find_argument(const std::string& flag) const;
    std::string normalize_flag(const std::string& flag) const;
};

/**
 * Table formatter for console output
 */
class TableFormatter {
public:
    TableFormatter();

    /**
     * Set column headers
     */
    void set_headers(const std::vector<std::string>& headers);

    /**
     * Add a row
     */
    void add_row(const std::vector<std::string>& row);

    /**
     * Print table to stdout
     */
    void print() const;

    /**
     * Get formatted table as string
     */
    std::string to_string() const;

    /**
     * Set column alignment (left, right, center)
     */
    void set_alignment(int column, const std::string& alignment);

private:
    std::vector<std::string> headers_;
    std::vector<std::vector<std::string>> rows_;
    std::map<int, std::string> alignments_;
    std::vector<size_t> column_widths_;

    void calculate_widths();
    std::string format_cell(const std::string& content, size_t width, const std::string& alignment) const;
};

/**
 * Progress indicator for long-running operations
 */
class ProgressIndicator {
public:
    ProgressIndicator(const std::string& message, int total);

    /**
     * Update progress
     */
    void update(int current);

    /**
     * Mark as complete
     */
    void complete();

    /**
     * Set custom completion message
     */
    void complete(const std::string& message);

private:
    std::string message_;
    int total_;
    int current_;
    bool completed_;
};

/**
 * Input validation utilities
 */
class Validator {
public:
    /**
     * Validate file exists and is readable
     */
    static bool is_valid_file(const std::string& filepath, std::string* error = nullptr);

    /**
     * Validate directory exists
     */
    static bool is_valid_directory(const std::string& dirpath, std::string* error = nullptr);

    /**
     * Validate number is in range
     */
    static bool is_in_range(int value, int min, int max, std::string* error = nullptr);

    /**
     * Validate string is not empty
     */
    static bool is_not_empty(const std::string& value, std::string* error = nullptr);

    /**
     * Validate string matches pattern (basic wildcard matching)
     */
    static bool matches_pattern(const std::string& value, const std::string& pattern, std::string* error = nullptr);
};

} // namespace cli

#endif // CLI_UTILS_HPP
