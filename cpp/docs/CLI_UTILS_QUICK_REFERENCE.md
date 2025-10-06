# CLI Utils Library - Quick Reference

## Include
```cpp
#include "cli_utils.hpp"
```

## Input Parsing (Main Feature)

### Parse Any Input Format
```cpp
auto result = cli::InputParser::parse(input);

if (result.success) {
    // result.values contains parsed values
    // result.type tells you what format it was
    for (const auto& val : result.values) {
        process(val);
    }
} else {
    std::cerr << result.error_message << std::endl;
}
```

### Input Format Examples
| Format | Example | Description |
|--------|---------|-------------|
| Direct list | `"A,B,C"` | Comma-separated values |
| Text file | `"file.txt"` | All lines |
| Text file (limited) | `"file.txt:10"` | First 10 lines |
| CSV file | `"file.csv:col"` | All rows from column |
| CSV file (limited) | `"file.csv:col:10"` | First 10 rows |

### Check Input Type
```cpp
switch (result.type) {
    case cli::InputParser::InputType::DIRECT_LIST:
        std::cout << "Direct list\n";
        break;
    case cli::InputParser::InputType::TEXT_FILE:
        std::cout << "File: " << result.filepath << "\n";
        std::cout << "Limit: " << result.limit << "\n";
        break;
    case cli::InputParser::InputType::CSV_FILE:
        std::cout << "File: " << result.filepath << "\n";
        std::cout << "Column: " << result.column_name << "\n";
        std::cout << "Limit: " << result.limit << "\n";
        break;
}
```

## Command-Line Arguments

### Setup
```cpp
cli::ArgumentParser parser(argv[0], "Tool description");

// Add required argument
parser.add_argument({
    "-p",              // short flag
    "--pairs",         // long flag
    "Description",     // help text
    true,              // required?
    true,              // has value?
    "",                // default value
    "SPEC"             // value name (for help)
});

// Add optional argument
parser.add_argument({
    "-o", "--output",
    "Output file",
    false,             // optional
    true,              // has value
    "output.csv",      // default
    "FILE"
});
```

### Parse & Use
```cpp
if (!parser.parse(argc, argv)) {
    // Error occurred or help shown
    if (!parser.get_errors().empty()) {
        for (const auto& err : parser.get_errors()) {
            std::cerr << "Error: " << err << "\n";
        }
        return 1;
    }
    return 0;  // Help shown
}

std::string pairs = parser.get("-p");
std::string output = parser.get("-o");
bool has_output = parser.has("-o");
```

## String Utilities

```cpp
// Trim whitespace
std::string s = cli::StringUtils::trim("  hello  ");  // "hello"

// Split string
auto tokens = cli::StringUtils::split("a,b,c", ',');  // ["a","b","c"]

// Join strings
std::string s = cli::StringUtils::join({"a","b","c"}, ",");  // "a,b,c"

// Check start/end
bool b = cli::StringUtils::starts_with("hello", "he");  // true
bool b = cli::StringUtils::ends_with("file.txt", ".txt");  // true

// Case conversion
std::string s = cli::StringUtils::to_lower("HELLO");  // "hello"
std::string s = cli::StringUtils::to_upper("hello");  // "HELLO"
```

## CSV Parsing

```cpp
// Parse single column
auto values = cli::CSVParser::parse_column("data.csv", "symbol", 10);

// Get columns
auto cols = cli::CSVParser::get_columns("data.csv");

// Read entire CSV
auto data = cli::CSVParser::read_csv("data.csv", -1);
// Returns: map<string, vector<string>>

// Count rows
int rows = cli::CSVParser::count_rows("data.csv");
```

## Text File Parsing

```cpp
// Parse lines
auto lines = cli::TextFileParser::parse_lines("file.txt", 10);

// Parse all lines
auto lines = cli::TextFileParser::parse_lines("file.txt");

// Count lines
int count = cli::TextFileParser::count_lines("file.txt");
```

**Features**:
- Skips empty lines
- Skips comments (lines starting with `#`)
- Trims whitespace automatically

## List Parsing

```cpp
// Parse comma-separated
auto items = cli::ListParser::parse("a,b,c");

// Parse with custom delimiter
auto items = cli::ListParser::parse("a|b|c", '|');
```

## Validation

```cpp
std::string error;

// File validation
if (!cli::Validator::is_valid_file("data.csv", &error)) {
    std::cerr << error << std::endl;
}

// Directory validation
if (!cli::Validator::is_valid_directory("/path/to/dir", &error)) {
    std::cerr << error << std::endl;
}

// Range validation
if (!cli::Validator::is_in_range(value, 1, 100, &error)) {
    std::cerr << error << std::endl;
}

// Empty check
if (!cli::Validator::is_not_empty(str, &error)) {
    std::cerr << error << std::endl;
}

// Pattern matching (wildcards)
if (!cli::Validator::matches_pattern(filename, "*.txt", &error)) {
    std::cerr << error << std::endl;
}
```

## Common Patterns

### Pattern 1: Simple Tool with Input File
```cpp
int main(int argc, char* argv[]) {
    cli::ArgumentParser parser(argv[0], "Description");
    parser.add_argument({"-i", "--input", "Input", true, true, "", "FILE"});

    if (!parser.parse(argc, argv)) return 1;

    auto result = cli::InputParser::parse(parser.get("-i"));
    if (!result.success) {
        std::cerr << result.error_message << std::endl;
        return 1;
    }

    for (const auto& value : result.values) {
        process(value);
    }
}
```

### Pattern 2: Tool with Input/Output
```cpp
int main(int argc, char* argv[]) {
    cli::ArgumentParser parser(argv[0], "Description");
    parser.add_argument({"-i", "--input", "Input", true, true, "", "FILE"});
    parser.add_argument({"-o", "--output", "Output", false, true, "out.csv", "FILE"});

    if (!parser.parse(argc, argv)) return 1;

    auto result = cli::InputParser::parse(parser.get("-i"));
    if (!result.success) return 1;

    std::string output = parser.get("-o");

    // Process and save
}
```

### Pattern 3: Tool with Multiple Options
```cpp
int main(int argc, char* argv[]) {
    cli::ArgumentParser parser(argv[0], "Description");
    parser.add_argument({"-p", "--pairs", "Pairs", true, true, "", "SPEC"});
    parser.add_argument({"-d", "--depth", "Depth", false, true, "10", "NUM"});
    parser.add_argument({"-o", "--output", "Output", false, true, "out.csv", "FILE"});

    if (!parser.parse(argc, argv)) return 1;

    auto result = cli::InputParser::parse(parser.get("-p"));
    int depth = std::stoi(parser.get("-d"));
    std::string output = parser.get("-o");

    // Process
}
```

## Error Handling

### InputParser Errors
```cpp
auto result = cli::InputParser::parse(input);
if (!result.success) {
    std::cerr << "Error: " << result.error_message << std::endl;
    // error_message contains detailed error
}
```

### ArgumentParser Errors
```cpp
if (!parser.parse(argc, argv)) {
    auto errors = parser.get_errors();
    for (const auto& err : errors) {
        std::cerr << "Error: " << err << std::endl;
    }
    return 1;
}
```

### Validation Errors
```cpp
std::string error;
if (!cli::Validator::is_valid_file(file, &error)) {
    std::cerr << "Validation failed: " << error << std::endl;
}
```

## Building

### CMakeLists.txt
```cmake
# Add CLI utils library
add_library(cli_utils STATIC lib/cli_utils.cpp)

# Link to your executable
add_executable(my_tool my_tool.cpp)
target_link_libraries(my_tool cli_utils)
```

## Command-Line Examples

```bash
# Help
./tool -h
./tool --help

# Direct list
./tool -p "A,B,C"

# Text file (all)
./tool -p data.txt

# Text file (limited)
./tool -p data.txt:10

# CSV file (all)
./tool -p data.csv:column

# CSV file (limited)
./tool -p data.csv:column:10

# With output
./tool -p data.txt -o result.csv
```

## Text File Format

```
# Comments start with #
VALUE1
VALUE2

# Empty lines are skipped
VALUE3
VALUE4
```

## CSV File Format

```csv
column1,column2,column3
value1a,value1b,value1c
value2a,value2b,value2c
```

Specify column: `file.csv:column2`

## Tips

1. **Auto-detection**: InputParser automatically detects format
2. **Error messages**: Always check `result.success` and show `error_message`
3. **Help**: ArgumentParser handles `-h` and `--help` automatically
4. **Limits**: Use `:N` to limit rows/lines (e.g., `file.txt:10`)
5. **Whitespace**: TextFileParser trims whitespace automatically
6. **Comments**: Lines starting with `#` are skipped in text files
7. **Case-insensitive**: Text file extension detection is case-insensitive

## Namespace

All utilities are in the `cli` namespace:
```cpp
cli::StringUtils
cli::CSVParser
cli::TextFileParser
cli::ListParser
cli::InputParser
cli::ArgumentParser
cli::Validator
```

## Documentation

- Full API: `cli_utils.hpp`
- Examples: `retrieve_kraken_live_data_level1.cpp`
- Detailed guide: `CLI_UTILS_TEXT_FILE_SUPPORT.md`
- Refactoring summary: `REFACTORING_SUMMARY.md`
