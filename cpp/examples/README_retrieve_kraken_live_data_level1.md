# Kraken Live Data Retriever - Level 1

A production-ready command-line tool for retrieving real-time Level 1 ticker data from Kraken cryptocurrency exchange.

## Features

- **Flexible Pair Input**: Support for both direct pair lists and CSV file input
- **CSV Column Extraction**: Read trading pairs from any CSV column with optional row limiting
- **Real-time Updates**: Live streaming ticker data with immediate display
- **Data Persistence**: Saves all ticker updates to CSV for analysis
- **Graceful Shutdown**: Ctrl+C handling with proper cleanup
- **Performance**: Uses simdjson for fast JSON parsing and condition variables for responsive event handling

## Building

```bash
cd /export1/rocky/dev/kraken/cpp
mkdir -p build && cd build
cmake ..
cmake --build . --target retrieve_kraken_live_data_level1
```

The executable will be located at: `build/retrieve_kraken_live_data_level1`

## Usage

### 1. Direct Pair List (Comma-Separated)

Monitor specific trading pairs:

```bash
./retrieve_kraken_live_data_level1 -p "BTC/USD,ETH/USD,SOL/USD"
```

### 2. CSV File - All Rows

Read all pairs from a CSV column:

```bash
./retrieve_kraken_live_data_level1 -p /export1/rocky/dev/kraken/kraken_usd_volume.csv:pair
```

### 3. CSV File - Limited Rows

Read top N pairs from a CSV column:

```bash
# Monitor top 10 pairs by volume
./retrieve_kraken_live_data_level1 -p /export1/rocky/dev/kraken/kraken_usd_volume.csv:pair:10

# Monitor top 50 pairs
./retrieve_kraken_live_data_level1 -p kraken_usd_volume.csv:pair:50
```

### 4. Help

```bash
./retrieve_kraken_live_data_level1 -h
```

## Input Format

### Command-Line Argument: `-p <specification>`

**Format 1: Direct list**
```
-p "PAIR1,PAIR2,PAIR3,..."
```
Example: `-p "BTC/USD,ETH/USD,SOL/USD"`

**Format 2: CSV file (all rows)**
```
-p /path/to/file.csv:column_name
```
Example: `-p kraken_usd_volume.csv:pair`

**Format 3: CSV file (limited rows)**
```
-p /path/to/file.csv:column_name:limit
```
Example: `-p kraken_usd_volume.csv:pair:10`

### CSV File Requirements

- Must be a valid CSV file with header row
- Column names must match exactly (case-sensitive)
- Pairs should use Kraken's format (e.g., "BTC/USD", "ETH/EUR")

## Output

### Console Output

Real-time ticker updates are displayed as they arrive:

```
[UPDATE] BTC/USD | Last: $96234.5 | Bid: $96234.0 | Ask: $96235.0 | Vol24h: 1234.56
[UPDATE] ETH/USD | Last: $3567.8 | Bid: $3567.5 | Ask: $3568.0 | Vol24h: 45678.90
```

Periodic status updates every 30 seconds:

```
[STATUS] Running time: 30s | Updates: 245 | Pending: 0
```

### CSV Output File

All ticker data is saved to: `kraken_ticker_live_level1.csv`

**Columns:**
- timestamp
- pair
- bid
- bid_volume
- ask
- ask_volume
- last
- volume_today
- volume_24h
- vwap_today
- vwap_24h
- trades_today
- trades_24h
- low_today
- low_24h
- high_today
- high_24h
- open_today
- open_24h
- change_24h
- change_pct_24h

## Examples

### Example 1: Monitor Top 10 Highest Volume Pairs

```bash
# First, generate volume rankings
cd /export1/rocky/dev/kraken
python get_volume_all_pairs.py

# Then monitor top 10
./cpp/build/retrieve_kraken_live_data_level1 -p kraken_top500_usd_volume.csv:pair:10
```

### Example 2: Monitor Specific Pairs

```bash
./retrieve_kraken_live_data_level1 -p "BTC/USD,ETH/USD,SOL/USD,XRP/USD,ADA/USD"
```

### Example 3: Monitor All USD Pairs

Create a file `usd_pairs.csv`:
```csv
pair
BTC/USD
ETH/USD
SOL/USD
XRP/USD
ADA/USD
MATIC/USD
AVAX/USD
```

Then run:
```bash
./retrieve_kraken_live_data_level1 -p usd_pairs.csv:pair
```

## Architecture

The tool is built on top of the `KrakenWebSocketClientSimdjsonV2` library and uses:

- **WebSocket v2 API**: Kraken's latest WebSocket API for ticker data
- **simdjson**: Fast JSON parsing (3-4x faster than nlohmann/json)
- **Condition Variables**: Immediate event response (no polling delay)
- **Thread-Safe Queue**: Async data collection from WebSocket thread
- **Signal Handling**: Graceful shutdown on Ctrl+C

### Event Flow

```
WebSocket Thread                Main Thread
================                ===========
Receive message       →         Wait on condition variable
Parse with simdjson   →
Store in queue        →
Notify condition var  →  →  →   Wake up immediately
                                Process updates
                                Display to console
                                Save to CSV
```

## Performance Considerations

### Recommended Pair Limits

- **1-10 pairs**: Excellent real-time updates, minimal latency
- **10-50 pairs**: Good performance, slight console flooding
- **50-100 pairs**: Consider filtering console output
- **100+ pairs**: Console output may be overwhelming, but CSV saves all data

### Memory Usage

Approximate memory per pair:
- WebSocket connection: ~10KB
- Queue buffer: ~1KB per update
- CSV buffer: ~200 bytes per update

For 100 pairs receiving 1 update/second each:
- Memory usage: ~2-3 MB
- CSV growth: ~20 KB/second

## Troubleshooting

### Error: "Cannot open file"

Check that the CSV file path is correct:
```bash
ls -la /export1/rocky/dev/kraken/kraken_usd_volume.csv
```

### Error: "Column 'XXX' not found in CSV"

Check available columns:
```bash
head -1 /export1/rocky/dev/kraken/kraken_usd_volume.csv
```

Column names are case-sensitive.

### Error: "No valid pairs found"

Check CSV file format:
```bash
head -5 kraken_usd_volume.csv
```

Ensure:
- File has header row
- Pairs are in correct column
- Pairs use Kraken format (e.g., "BTC/USD")

### Connection Issues

Check internet connectivity and Kraken API status:
```bash
curl -s https://api.kraken.com/0/public/SystemStatus | jq
```

## Integration with Existing Scripts

### Volume Analysis Pipeline

```bash
# Step 1: Get volume rankings
python get_volume_all_pairs.py

# Step 2: Monitor top 20 live
./cpp/build/retrieve_kraken_live_data_level1 -p kraken_top500_usd_volume.csv:pair:20

# Step 3: Analyze live data
python analyze_ticker_data.py kraken_ticker_live_level1.csv
```

### Custom Pair Selection

```bash
# Extract specific pairs from volume file
awk -F',' '$1 ~ /USD$/ {print $1}' kraken_top500_usd_volume.csv > usd_pairs.csv

# Monitor them
./retrieve_kraken_live_data_level1 -p usd_pairs.csv:pair
```

## Future Enhancements

Planned features for future versions:

- **Level 2 (Order Book)**: `retrieve_kraken_live_data_level2.cpp`
- **Trades Stream**: `retrieve_kraken_live_data_trades.cpp`
- **OHLC Data**: `retrieve_kraken_live_data_ohlc.cpp`
- **Multiple Output Formats**: JSON, Parquet, SQLite
- **Filtering**: By volume, volatility, spread
- **Alerts**: Price thresholds, volume spikes

## Related Files

- Source: `cpp/examples/retrieve_kraken_live_data_level1.cpp`
- Library: `cpp/lib/kraken_websocket_client_simdjson_v2.hpp`
- CMake: `cpp/CMakeLists.txt`
- Example: `cpp/examples/example_integration_cond.cpp`

## License

Part of the Kraken API integration project.
