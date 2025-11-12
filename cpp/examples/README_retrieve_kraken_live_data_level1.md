# Kraken Live Data Retriever - Level 1

A production-ready command-line tool for retrieving real-time Level 1 ticker data from Kraken cryptocurrency exchange.

## Features

- **Flexible Pair Input**: Support for both direct pair lists and CSV file input
- **CSV Column Extraction**: Read trading pairs from any CSV column with optional row limiting
- **Real-time Updates**: Live streaming ticker data with immediate display
- **Data Persistence**: Saves all ticker updates to CSV for analysis
- **Memory-Efficient Flushing**: Periodic data flush to prevent unbounded memory growth
- **Time-Based Segmentation**: Automatic hourly/daily file splitting for long-running collection
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

### 4. Custom Output File

```bash
./retrieve_kraken_live_data_level1 -p "BTC/USD" -o my_data.csv
```

### 5. Hourly File Segmentation (Long-Running Collection)

```bash
# Creates output.20251112_10.csv, output.20251112_11.csv, etc.
./retrieve_kraken_live_data_level1 -p "BTC/USD,ETH/USD" --hourly
```

### 6. Daily File Segmentation

```bash
# Creates output.20251112.csv, output.20251113.csv, etc.
./retrieve_kraken_live_data_level1 -p "BTC/USD" --daily
```

### 7. Custom Flush Settings

```bash
# Flush every 10 seconds OR when memory exceeds 5MB
./retrieve_kraken_live_data_level1 -p "BTC/USD" -f 10 -m 5242880
```

### 8. Help

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

## Command-Line Options

### Required Arguments

| Option | Description | Example |
|--------|-------------|---------|
| `-p, --pairs` | Pairs specification (direct list or CSV file) | `-p "BTC/USD,ETH/USD"` |

### Optional Arguments

| Option | Type | Default | Description |
|--------|------|---------|-------------|
| `-o, --output` | FILE | `kraken_ticker_live_level1.csv` | Output CSV filename |
| `-f, --flush-interval` | SECONDS | `30` | Flush interval (0 = disabled) |
| `-m, --memory-threshold` | BYTES | `10485760` | Memory threshold (0 = disabled) |
| `--hourly` | flag | off | Enable hourly file segmentation |
| `--daily` | flag | off | Enable daily file segmentation |

### Flush Behavior

The tool uses **hybrid flushing** (OR logic):
- Flushes every `-f` seconds, OR
- Flushes when memory exceeds `-m` bytes
- Whichever condition is met first triggers the flush

**Common configurations**:
```bash
# Default: Flush every 30s OR 10MB
./retrieve_kraken_live_data_level1 -p "BTC/USD"

# Aggressive: Flush every 5s OR 5MB (low memory)
./retrieve_kraken_live_data_level1 -p "BTC/USD" -f 5 -m 5242880

# Memory-only: Flush only when exceeding 20MB
./retrieve_kraken_live_data_level1 -p "BTC/USD" -f 0 -m 20971520

# Time-only: Flush every 60s
./retrieve_kraken_live_data_level1 -p "BTC/USD" -f 60 -m 0
```

### File Segmentation

**Hourly Mode** (`--hourly`):
- Creates one file per hour (UTC timezone)
- Format: `output.YYYYMMDD_HH.csv`
- Example: `kraken_ticker_live_level1.20251112_10.csv`

**Daily Mode** (`--daily`):
- Creates one file per day (UTC timezone)
- Format: `output.YYYYMMDD.csv`
- Example: `kraken_ticker_live_level1.20251112.csv`

**Note**: `--hourly` and `--daily` are mutually exclusive.

## Output

### Console Output

Real-time ticker updates are displayed as they arrive:

```
[UPDATE] BTC/USD | Last: $96234.5 | Bid: $96234.0 | Ask: $96235.0 | Vol24h: 1234.56
[UPDATE] ETH/USD | Last: $3567.8 | Bid: $3567.5 | Ask: $3568.0 | Vol24h: 45678.90
```

Periodic status updates every 30 seconds:

```
[STATUS] Running time: 90s | Updates: 342 | Flushes: 3 | Memory: 0.5MB | Pending: 12
```

With segmentation enabled:
```
[STATUS] Running time: 3700s | Updates: 15420 | Flushes: 123 | Memory: 0.5MB | Pending: 12
         Current file: kraken_ticker_live_level1.20251112_10.csv (3 files created)
```

Flush notifications:
```
[FLUSH] Wrote 234 records to kraken_ticker_live_level1.csv
```

Segment transition notifications:
```
[SEGMENT] Starting new file: kraken_ticker_live_level1.20251112_11.csv
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

### Example 4: Long-Running Collection (24/7 Monitoring)

```bash
# Hourly files with default flush settings
./retrieve_kraken_live_data_level1 -p "BTC/USD,ETH/USD" --hourly

# Creates:
# kraken_ticker_live_level1.20251112_10.csv
# kraken_ticker_live_level1.20251112_11.csv
# kraken_ticker_live_level1.20251112_12.csv
# ...
```

### Example 5: Memory-Constrained Environment

```bash
# Aggressive flushing: every 5s OR 2MB
./retrieve_kraken_live_data_level1 \
  -p "@top100.csv:pair:100" \
  --hourly \
  -f 5 \
  -m 2097152
```

### Example 6: Daily Aggregation Reports

```bash
# One file per day for daily reports
./retrieve_kraken_live_data_level1 \
  -p "BTC/USD,ETH/USD,SOL/USD" \
  --daily \
  -f 60
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

**With Periodic Flushing (Default)**:
- **Maximum memory**: Bounded by flush settings (default: 10MB)
- **Typical usage**: 2-5 MB (flushes every 30s or 10MB)
- **Recommended**: Always keep flush enabled for long-running collection

**Memory per pair** (when flush disabled):
- WebSocket connection: ~10KB
- Queue buffer: ~1KB per update
- CSV buffer: ~120 bytes per update

**Example calculations**:

| Pairs | Update Rate | Memory/Hour | Recommended Settings |
|-------|-------------|-------------|---------------------|
| 10 | 1/sec | ~4 MB | Default (30s, 10MB) |
| 50 | 1/sec | ~20 MB | Default or -f 15 |
| 100 | 1/sec | ~40 MB | -f 10 -m 10485760 --hourly |
| 500 | 1/sec | ~200 MB | -f 5 -m 5242880 --hourly |

### Disk Usage

**Without Segmentation**:
- Single file grows continuously
- ~150 bytes per update (CSV overhead)
- Example: 100 pairs × 1 update/sec × 3600 sec = ~54 MB/hour

**With Hourly Segmentation**:
- One file per hour
- Easier to process and archive
- Example: ~54 MB/hour per file × 24 = 24 files/day

**With Daily Segmentation**:
- One file per day
- Good for daily aggregation
- Example: ~1.3 GB/day per file

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

### Memory Usage Too High

Reduce flush interval or memory threshold:
```bash
# Flush every 10 seconds
./retrieve_kraken_live_data_level1 -p "..." -f 10

# Or flush at 5MB instead of 10MB
./retrieve_kraken_live_data_level1 -p "..." -m 5242880
```

### Too Many Segment Files

Switch from hourly to daily segmentation:
```bash
# Use daily instead of hourly
./retrieve_kraken_live_data_level1 -p "..." --daily
```

### Files Too Large

Switch from daily to hourly segmentation:
```bash
# Use hourly instead of daily
./retrieve_kraken_live_data_level1 -p "..." --hourly
```

### Error: "--hourly and --daily cannot be used together"

Remove one of the segmentation flags:
```bash
# Choose one
./retrieve_kraken_live_data_level1 -p "..." --hourly
# OR
./retrieve_kraken_live_data_level1 -p "..." --daily
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

- **Additional Segment Intervals**: 6-hour, 30-minute segments
- **Automatic Compression**: Auto-compress completed segments
- **Auto-Cleanup**: Delete old segments after N days
- **Level 2 (Order Book)**: `retrieve_kraken_live_data_level2.cpp`
- **Trades Stream**: `retrieve_kraken_live_data_trades.cpp`
- **OHLC Data**: `retrieve_kraken_live_data_ohlc.cpp`
- **Multiple Output Formats**: JSON, Parquet, SQLite
- **Filtering**: By volume, volatility, spread
- **Alerts**: Price thresholds, volume spikes

## Related Files

### Source Code
- Main program: `cpp/examples/retrieve_kraken_live_data_level1.cpp`
- Base library: `cpp/lib/kraken_websocket_client_base.hpp`
- simdjson client: `cpp/lib/kraken_websocket_client_simdjson_v2.hpp`
- Build config: `cpp/CMakeLists.txt`

### Documentation
- **This README**: Usage guide and examples
- **[MEMORY_MANAGEMENT_AND_SEGMENTATION.md](../docs/MEMORY_MANAGEMENT_AND_SEGMENTATION.md)**: Technical design decisions and architecture
- **[MEMORY_MANAGEMENT_QUICK_REFERENCE.md](../docs/MEMORY_MANAGEMENT_QUICK_REFERENCE.md)**: Quick reference for flush and segmentation options
- **[MEMORY_MANAGEMENT_CHANGELOG.md](../docs/MEMORY_MANAGEMENT_CHANGELOG.md)**: Detailed code changes and migration guide

## License

Part of the Kraken API integration project.
