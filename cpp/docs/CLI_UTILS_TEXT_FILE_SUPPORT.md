# CLI Utils Library - Text File Support

## Enhancement Summary

Added text file (*.txt) support to the `cli_utils` library's `InputParser`, enabling simple one-value-per-line input files without headers.

## Changes Made

### 1. New Class: `TextFileParser`

**Location**: `lib/cli_utils.hpp` and `lib/cli_utils.cpp`

**Functions**:
```cpp
class TextFileParser {
public:
    // Parse text file and read values (one per line, no header)
    static std::vector<std::string> parse_lines(
        const std::string& filepath,
        int limit = -1  // -1 = all lines
    );

    // Count non-empty lines in text file
    static int count_lines(const std::string& filepath);
};
```

**Features**:
- Reads one value per line
- No header row required
- Automatically trims whitespace
- Skips empty lines
- Skips comment lines (lines starting with `#`)
- Supports limiting number of lines

### 2. Enhanced `InputParser`

**New InputType**:
```cpp
enum class InputType {
    DIRECT_LIST,    // "VALUE1,VALUE2,VALUE3"
    CSV_FILE,       // "file.csv:column:limit"
    TEXT_FILE,      // "file.txt" or "file.txt:limit"  ← NEW
    UNKNOWN
};
```

**Auto-detection Logic**:
1. Check if input contains file path indicators (`/`, `.`, `:`)
2. Extract filepath (before any colon)
3. If filepath ends with `.txt` → `TextFileParser`
4. Otherwise → `CSVParser`

**Format Support**:
- `file.txt` - Read all lines
- `file.txt:10` - Read first 10 lines
- `file.txt:100` - Read first 100 lines

### 3. Updated Applications

**`retrieve_kraken_live_data_level1.cpp`**:
- Added TEXT_FILE case to input source display
- Updated help examples to show text file usage

## Usage Examples

### Direct API Usage

```cpp
#include "cli_utils.hpp"

// Parse text file - all lines
auto lines = cli::TextFileParser::parse_lines("tickers.txt");

// Parse text file - first 10 lines
auto lines = cli::TextFileParser::parse_lines("tickers.txt", 10);

// Count lines
int count = cli::TextFileParser::count_lines("tickers.txt");
```

### InputParser Usage

```cpp
#include "cli_utils.hpp"

// Parse any format automatically
auto result = cli::InputParser::parse("kraken_tickers.txt:10");

if (result.success) {
    std::cout << "Type: TEXT_FILE\n";
    std::cout << "File: " << result.filepath << "\n";
    std::cout << "Limit: " << result.limit << "\n";
    std::cout << "Values: " << result.values.size() << "\n";

    for (const auto& value : result.values) {
        std::cout << "  - " << value << "\n";
    }
}
```

### Command-Line Tool Usage

```bash
# All three input methods now supported:

# 1. Direct list
./retrieve_kraken_live_data_level1 -p "BTC/USD,ETH/USD,SOL/USD"

# 2. Text file (NEW!)
./retrieve_kraken_live_data_level1 -p kraken_tickers.txt
./retrieve_kraken_live_data_level1 -p kraken_tickers.txt:10
./retrieve_kraken_live_data_level1 -p /export1/rocky/dev/kraken/kraken_tickers.txt:5

# 3. CSV file
./retrieve_kraken_live_data_level1 -p kraken_usd_volume.csv:pair:10
```

## Text File Format

### Simple Format (One Value Per Line)

**Example: `kraken_tickers.txt`**
```
USDT/USD
ETH/USD
USDC/EUR
XBT/USD
USDC/USD
USDT/EUR
SOL/USD
XRP/USD
```

**Features**:
- One value per line
- No header row needed
- Empty lines are skipped
- Leading/trailing whitespace is trimmed
- Comments (lines starting with `#`) are ignored

### With Comments

**Example: `my_pairs.txt`**
```
# Major cryptocurrencies
BTC/USD
ETH/USD

# Stablecoins
USDT/USD
USDC/USD

# Altcoins
SOL/USD
XRP/USD
```

All lines starting with `#` are automatically skipped.

## Comparison: Text File vs CSV File

### Text File (*.txt)
```
BTC/USD
ETH/USD
SOL/USD
```
- **Pros**: Simple, no header, easy to create/edit
- **Cons**: Single column only
- **Format**: `file.txt` or `file.txt:limit`
- **Use Case**: Simple lists of symbols/pairs

### CSV File (*.csv)
```csv
pair,base_volume_24h,quote_volume_24h,usd_volume_24h
BTC/USD,1111.38,127114685.47,127114685.47
ETH/USD,31092.71,129671245.29,129671245.29
SOL/USD,265796.30,55660404.75,55660404.75
```
- **Pros**: Multiple columns, structured data
- **Cons**: Requires header, column specification needed
- **Format**: `file.csv:column_name` or `file.csv:column_name:limit`
- **Use Case**: Data with metadata (volume, price, etc.)

## Real-World Testing

### Test 1: Small Text File (5 lines)
```bash
$ ./retrieve_kraken_live_data_level1 -p /export1/rocky/dev/kraken/kraken_tickers.txt:5

Input source: Text file: /export1/rocky/dev/kraken/kraken_tickers.txt [limit: 5]
Output file: kraken_ticker_live_level1.csv

Subscribing to 5 pairs:
  - USDT/USD
  - ETH/USD
  - USDC/EUR
  - XBT/USD
  - USDC/USD

✅ SUCCESS
```

### Test 2: Large Text File (All 1253 lines)
```bash
$ ./retrieve_kraken_live_data_level1 -p /export1/rocky/dev/kraken/kraken_tickers.txt

Input source: Text file: /export1/rocky/dev/kraken/kraken_tickers.txt
Output file: kraken_ticker_live_level1.csv

Subscribing to 1253 pairs:
  - USDT/USD
  - ETH/USD
  - USDC/EUR
  ...
  ... and 1243 more

✅ SUCCESS - All 1253 pairs subscribed
```

## Implementation Details

### File Type Detection

```cpp
bool InputParser::is_text_file(const std::string& filepath) {
    return StringUtils::ends_with(StringUtils::to_lower(filepath), ".txt");
}
```

Simple extension-based detection. Case-insensitive.

### Parsing Logic

```cpp
std::vector<std::string> TextFileParser::parse_lines(
    const std::string& filepath,
    int limit
) {
    std::vector<std::string> lines;
    std::ifstream file(filepath);

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

    return lines;
}
```

Key features:
- Respects limit parameter
- Trims whitespace
- Filters empty lines
- Filters comment lines

### ParseResult for Text Files

```cpp
InputParser::ParseResult result;
result.type = InputType::TEXT_FILE;
result.filepath = "kraken_tickers.txt";
result.limit = 10;  // or -1 for all
result.column_name = "";  // Not used for text files
result.values = ["USDT/USD", "ETH/USD", ...];
result.success = true;
result.error_message = "";
```

## Error Handling

### File Not Found
```bash
$ ./retrieve_kraken_live_data_level1 -p nonexistent.txt:10
Error: Cannot open file: nonexistent.txt
```

### Empty File
```bash
$ ./retrieve_kraken_live_data_level1 -p empty.txt
Error: No values extracted from text file
```

### Invalid Limit
```bash
$ ./retrieve_kraken_live_data_level1 -p file.txt:abc
Error: Invalid limit: abc
```

## Benefits

1. **Simplicity**: No need for CSV headers or column specifications
2. **Flexibility**: Easy to create text files manually or from scripts
3. **Compatibility**: Works with existing `InputParser` interface
4. **Robustness**: Handles comments, whitespace, empty lines gracefully
5. **Performance**: Efficient line-by-line reading with optional limit

## Creating Text Files from Other Sources

### From CSV (extract one column)
```bash
# Extract 'pair' column from CSV (skip header)
tail -n +2 kraken_usd_volume.csv | cut -d',' -f1 > kraken_tickers.txt
```

### From Python
```python
import pandas as pd

df = pd.read_csv('kraken_usd_volume.csv')
df['pair'].to_csv('kraken_tickers.txt', index=False, header=False)
```

### From Shell Script
```bash
cat > my_pairs.txt << 'EOF'
BTC/USD
ETH/USD
SOL/USD
EOF
```

### From Another Program's Output
```bash
# If you have a program that outputs pairs
./my_program --list-pairs > pairs.txt

# Then use it directly
./retrieve_kraken_live_data_level1 -p pairs.txt
```

## Future Enhancements

Potential improvements:

1. **Support multiple file extensions**: `.list`, `.dat`, etc.
2. **Inline comments**: `BTC/USD  # Bitcoin`
3. **Blank line preservation option**: For formatting
4. **Case transformation**: Auto-convert to uppercase/lowercase
5. **Validation**: Check format of each line (e.g., must contain `/`)
6. **Deduplication**: Skip duplicate entries

## Backward Compatibility

All existing functionality remains unchanged:
- Direct list parsing: ✅ Same
- CSV file parsing: ✅ Same
- Error messages: ✅ Enhanced but compatible

No breaking changes to API or command-line interface.

## Summary

Text file support makes the CLI utilities even more user-friendly:

| Format      | Example                        | Best For              |
|-------------|--------------------------------|-----------------------|
| Direct List | `"BTC/USD,ETH/USD"`           | Quick tests (2-5)     |
| Text File   | `tickers.txt:10`              | Simple lists (5-100)  |
| CSV File    | `data.csv:pair:10`            | Structured data       |

All three formats are now seamlessly supported through a single unified `InputParser::parse()` interface.
