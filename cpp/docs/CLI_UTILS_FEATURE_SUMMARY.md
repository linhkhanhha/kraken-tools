# Feature Summary: CLI Utils Library with Text File Support

## What We Built

A reusable **CLI utilities library** that makes building user-friendly command-line tools easy and consistent.

## Key Features

### 1. Three Input Methods (All Auto-Detected!)

Your tools can now accept input in **three flexible formats**:

#### Direct List (Comma-Separated)
```bash
./retrieve_kraken_live_data_level1 -p "BTC/USD,ETH/USD,SOL/USD"
```
- **Best for**: Quick tests, 2-5 pairs
- **Format**: Values separated by commas

#### Text File (One Per Line)
```bash
./retrieve_kraken_live_data_level1 -p kraken_tickers.txt
./retrieve_kraken_live_data_level1 -p kraken_tickers.txt:10
```
- **Best for**: Simple lists, 5-100+ pairs
- **Format**: One value per line, no header needed
- **Features**:
  - Skips empty lines
  - Skips comments (lines starting with `#`)
  - Trims whitespace
  - Optional limit (`:10` = first 10 lines)

#### CSV File (Structured Data)
```bash
./retrieve_kraken_live_data_level1 -p kraken_usd_volume.csv:pair
./retrieve_kraken_live_data_level1 -p kraken_usd_volume.csv:pair:10
```
- **Best for**: Data with metadata (volume, price, etc.)
- **Format**: CSV with headers, specify column name
- **Optional limit**: `:10` = first 10 rows

### 2. Professional Argument Parsing

```cpp
cli::ArgumentParser parser(argv[0], "Tool description");

parser.add_argument({
    "-p", "--pairs",
    "Pairs specification",
    true,   // required
    true,   // has value
    "",     // default
    "SPEC"  // help name
});

if (!parser.parse(argc, argv)) {
    // Handles errors and -h/--help automatically
}

std::string value = parser.get("-p");
```

**Features**:
- Auto-generates help messages
- Handles `-h` and `--help` flags
- Shows required vs optional arguments
- Displays default values
- Clear error messages

### 3. Unified Input Parser

**One API, three input formats:**

```cpp
auto result = cli::InputParser::parse(user_input);

if (result.success) {
    // Auto-detected and parsed any format!
    for (const auto& value : result.values) {
        use(value);
    }
}
```

The library **automatically detects** which format is being used!

## Real-World Example

### Before (Manual Parsing - 150 lines)
```cpp
// Custom parsing code
// CSV handling
// Error checking
// Help messages
// ... 150 lines of boilerplate ...
```

### After (Using CLI Utils - 10 lines)
```cpp
#include "cli_utils.hpp"

cli::ArgumentParser parser(argv[0], "Description");
parser.add_argument({"-p", "--pairs", "Pairs", true, true, "", "SPEC"});
if (!parser.parse(argc, argv)) return 1;

auto result = cli::InputParser::parse(parser.get("-p"));
if (!result.success) {
    std::cerr << result.error_message << std::endl;
    return 1;
}

// Use result.values directly!
```

## Complete Library API

### StringUtils
```cpp
cli::StringUtils::trim("  hello  ");              // "hello"
cli::StringUtils::split("a,b,c", ',');           // ["a", "b", "c"]
cli::StringUtils::join({"a", "b"}, ",");         // "a,b"
cli::StringUtils::starts_with("hello", "he");    // true
cli::StringUtils::ends_with("file.txt", ".txt"); // true
cli::StringUtils::to_lower("HELLO");             // "hello"
cli::StringUtils::to_upper("hello");             // "HELLO"
```

### CSVParser
```cpp
// Parse single column
auto values = cli::CSVParser::parse_column("file.csv", "pair", 10);

// Get column names
auto columns = cli::CSVParser::get_columns("file.csv");

// Read entire CSV
auto data = cli::CSVParser::read_csv("file.csv");
```

### TextFileParser (NEW!)
```cpp
// Parse text file
auto lines = cli::TextFileParser::parse_lines("file.txt", 10);

// Count lines
int count = cli::TextFileParser::count_lines("file.txt");
```

### Validator
```cpp
std::string error;
if (!cli::Validator::is_valid_file("file.csv", &error)) {
    std::cerr << error << std::endl;
}

if (!cli::Validator::is_in_range(value, 1, 100, &error)) {
    std::cerr << error << std::endl;
}
```

## Usage in retrieve_kraken_live_data_level1

### All Input Methods Work!

```bash
# Method 1: Direct list
./retrieve_kraken_live_data_level1 -p "BTC/USD,ETH/USD,SOL/USD"
# Input source: Direct list (3 pairs)

# Method 2: Text file (first 5)
./retrieve_kraken_live_data_level1 -p kraken_tickers.txt:5
# Input source: Text file: kraken_tickers.txt [limit: 5]

# Method 3: Text file (all 1253 pairs!)
./retrieve_kraken_live_data_level1 -p kraken_tickers.txt
# Input source: Text file: kraken_tickers.txt
# Subscribing to 1253 pairs

# Method 4: CSV file
./retrieve_kraken_live_data_level1 -p kraken_usd_volume.csv:pair:10
# Input source: CSV file: kraken_usd_volume.csv [column: pair, limit: 10]

# Custom output file
./retrieve_kraken_live_data_level1 -p "BTC/USD,ETH/USD" -o my_data.csv
# Output file: my_data.csv
```

## Creating Text Files

### From Scratch
```bash
cat > my_pairs.txt << 'EOF'
# My favorite pairs
BTC/USD
ETH/USD
SOL/USD
EOF
```

### From CSV
```bash
tail -n +2 kraken_usd_volume.csv | cut -d',' -f1 > tickers.txt
```

### From Python
```python
pairs = ['BTC/USD', 'ETH/USD', 'SOL/USD']
with open('pairs.txt', 'w') as f:
    f.write('\n'.join(pairs))
```

## Benefits

1. **Code Reuse**: Write once, use in all tools
2. **Consistency**: Same interface across all tools
3. **User-Friendly**: Supports multiple input formats automatically
4. **Less Code**: ~150 lines saved per tool
5. **Better Errors**: Clear, helpful error messages
6. **Maintainable**: Fix bugs once, benefits all tools
7. **Testable**: Library can be unit tested independently

## Future Tools Can Use This

Any new tool can leverage the library:

```cpp
// Order book tool
./retrieve_kraken_orderbook -p tickers.txt:10 -d 25

// Trades tool
./retrieve_kraken_trades -p "BTC/USD,ETH/USD" -o trades.csv

// OHLC tool
./retrieve_kraken_ohlc -p volume.csv:pair:20 -i 1m
```

All with consistent command-line interface!

## Files

### Library Files
- `cpp/lib/cli_utils.hpp` - Header (370 lines)
- `cpp/lib/cli_utils.cpp` - Implementation (650 lines)

### Documentation
- `cpp/REFACTORING_SUMMARY.md` - Complete refactoring overview
- `cpp/CLI_UTILS_TEXT_FILE_SUPPORT.md` - Text file feature details
- `FEATURE_SUMMARY.md` - This file

### Updated Application
- `cpp/examples/retrieve_kraken_live_data_level1.cpp` - Now uses library

## Summary Table

| Feature | Before | After |
|---------|--------|-------|
| Input formats | 1 (hardcoded) | 3 (auto-detected) |
| Lines of parsing code | ~150 | ~10 |
| Help message | Manual | Auto-generated |
| Error messages | Basic | Detailed |
| Argument parsing | Manual loops | ArgumentParser |
| Reusable | No | Yes (library) |
| Text file support | No | ✅ Yes |
| CSV file support | Manual | ✅ Library |
| Custom output file | No | ✅ Yes (-o flag) |

## Next Steps

1. **Use in other tools** - Apply to Level 2, trades, OHLC tools
2. **Add features** - More validators, formatters, progress bars
3. **Unit tests** - Test library functions independently
4. **Documentation** - Doxygen API docs

## Conclusion

The CLI utilities library transforms your command-line tools from basic scripts into professional, user-friendly applications with minimal code!

**One library. Three input formats. Infinite possibilities.**
