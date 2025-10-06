# Level 2 Order Book Tool - Design Document

## Overview

This document captures the brainstorming session and design decisions for the Level 2 (order book) data capture tool for Kraken WebSocket v2.

**Date:** October 3, 2025
**Tool Name:** `retrieve_kraken_live_data_level2`
**Based on:** Kraken WebSocket v2 Book Channel (https://docs.kraken.com/api/docs/websocket-v2/book)

## Background

### Kraken Book Channel Details

**Endpoint:** `wss://ws.kraken.com/v2`
**Channel:** `book`
**Purpose:** Stream Level 2 (L2) aggregated order book data

**Subscription Parameters:**
- `symbol`: Array of trading pairs (e.g., ["BTC/USD", "ETH/USD"])
- `depth`: Number of price levels (10, 25, 100, 500, 1000; default 10)
- `snapshot`: Boolean to request initial snapshot (default true)

**Message Types:**
1. **Snapshot** - Initial full order book state
2. **Update** - Incremental changes to the order book

**Data Structure:**
- `bids`: Array of [price, quantity] pairs
- `asks`: Array of [price, quantity] pairs
- `checksum`: CRC32 checksum for validation
- `timestamp`: Message timestamp

## Design Questions & Decisions

### Question 1: Order Book Depth

**Question:** Should depth be configurable? What default?

**Options Considered:**
- Fixed depth (10)
- Configurable depth with default

**Decision:** ✅ **Configurable with default of 10**

**Rationale:**
- Different use cases need different depths (scalping vs analysis)
- Kraken supports: 10, 25, 100, 500, 1000
- Default of 10 matches Kraken's default and is suitable for most use cases

**Implementation:**
```bash
-d, --depth <NUM>    Order book depth (10, 25, 100, 500, 1000)
                     Default: 10
```

---

### Question 2: Data to Capture

**Question:** What data should we save?

**Options Considered:**

**A) Save Everything (Snapshots + Updates)**
- Raw message stream preserved
- Can replay order book exactly
- More data volume

**B) Maintain State (Apply updates, save periodically)**
- Less data volume
- Current state only
- Can't replay exact history

**C) Both**
- Maximum flexibility
- Most complex
- Highest burden on capture tool

**Decision:** ✅ **Option A (raw data) + Separate processing tool**

**Rationale:**
- Separation of concerns: capture vs processing
- Raw data preserved for multiple analysis passes
- Processing tool can create different snapshot intervals from same raw data
- Keeps capture tool fast and lightweight

**Two-Tool Approach:**

**Tool 1: `retrieve_kraken_live_data_level2`** (Capture)
- Saves raw snapshots + updates
- No processing burden
- Fast, reliable capture

**Tool 2: `process_orderbook_snapshots`** (Analysis - Future)
- Reads saved .jsonl files
- Rebuilds order book state
- Creates periodic snapshots (1s, 5s, 1m intervals)
- Can run multiple times on same data

**Workflow Example:**
```bash
# Step 1: Capture raw data
./retrieve_kraken_live_data_level2 -p "BTC/USD" -o btc_raw.jsonl

# Step 2: Generate 1-second snapshots
./process_orderbook_snapshots -i btc_raw.jsonl --interval 1s -o btc_1s.csv

# Step 3: Generate 5-second snapshots (from same raw data)
./process_orderbook_snapshots -i btc_raw.jsonl --interval 5s -o btc_5s.csv
```

---

### Question 3: Output Format

**Question:** What file format for raw data?

**Options Considered:**

**A) JSON Lines (.jsonl)** - One JSON object per line
```
{"timestamp":"...","type":"snapshot","data":{...}}
{"timestamp":"...","type":"update","data":{...}}
```
- Pros: Streamable, appendable, standard format
- Cons: Not pretty-printed

**B) Pretty JSON (.json)** - Array of objects
```json
[
  {"timestamp":"...","type":"snapshot"},
  {"timestamp":"...","type":"update"}
]
```
- Pros: Human readable
- Cons: Can't append, memory intensive

**Decision:** ✅ **JSON Lines (.jsonl)**

**Rationale:**
- Industry standard for streaming/log data
- Memory efficient (process line-by-line)
- Appendable (can restart capture)
- Perfect for processing tool to read
- Tools like `jq` work great with it

**Example Output:**
```jsonl
{"timestamp":"2025-10-03T14:00:00.123Z","channel":"book","type":"snapshot","data":{"symbol":"BTC/USD","bids":[[123900.0,1.5],[123899.5,2.3]],"asks":[[123901.0,1.2]],"checksum":1234567890}}
{"timestamp":"2025-10-03T14:00:00.456Z","channel":"book","type":"update","data":{"symbol":"BTC/USD","bids":[[123900.0,1.8]],"checksum":1234567891}}
```

---

### Question 4: Checksum Validation

**Question:** Should we validate Kraken's CRC32 checksums?

**Options Considered:**

**A) No validation**
- Faster
- Simpler
- Won't detect corruption

**B) Validate and warn**
- Detect issues immediately
- Log warnings but continue
- Slight overhead

**C) Validate and stop on error**
- Ensures data quality
- Stops on any error
- Too strict for capture tool

**Decision:** ✅ **Option B (validate and warn) by default + skip flag**

**Rationale:**
- Know immediately if data is corrupted
- Warnings logged but capture continues
- Users can review warnings later
- Option to skip validation for maximum speed

**Implementation:**
```bash
# Default: validate with warnings
./retrieve_kraken_live_data_level2 -p "BTC/USD"

# Skip validation (faster)
./retrieve_kraken_live_data_level2 -p "BTC/USD" --skip-validation
```

**Warning Example:**
```
[WARNING] Checksum mismatch for BTC/USD: expected 1234567890, got 1234567891
[UPDATE] BTC/USD: 1 bid changed, 0 asks changed
```

---

### Question 5: Display/Monitoring

**Question:** What should the tool show while running?

**Options Considered:**

**A) Update counters only**
```
[UPDATE] BTC/USD snapshot received (10 bids, 10 asks)
[UPDATE] BTC/USD update (1 bid changed)
```

**B) Show top-of-book (best bid/ask)**
```
[BTC/USD] Bid: $123900.0 (1.5) | Ask: $123901.0 (1.2) | Spread: $1.0
```

**C) Show full order book (terminal UI)**
```
BTC/USD Order Book (Depth: 10)
Bids                | Asks
$123900.0 [1.5] ███ | ██ [1.2] $123901.0
```

**D) Minimal (counts only)**
```
[STATUS] BTC/USD: 1 snapshot, 234 updates | ETH/USD: 1 snapshot, 187 updates
```

**Decision:** ✅ **Tiered approach - only compute what's requested**

**Rationale:**
- Different use cases need different verbosity
- Performance: only compute what user asks for
- Flexibility: combine flags as needed

**Implementation:**

**Default (Minimal - Fastest):**
```bash
./retrieve_kraken_live_data_level2 -p "BTC/USD,ETH/USD"
```
Output:
```
[STATUS] BTC/USD: 1 snapshot, 234 updates | ETH/USD: 1 snapshot, 187 updates
```

**Add Update Details:**
```bash
./retrieve_kraken_live_data_level2 -p "BTC/USD" --show-updates
# or
./retrieve_kraken_live_data_level2 -p "BTC/USD" -v
```
Output:
```
[SNAPSHOT] BTC/USD: 10 bids, 10 asks
[UPDATE] BTC/USD: 1 bid changed, 0 asks changed
[UPDATE] BTC/USD: 0 bids changed, 2 asks changed
```

**Add Top-of-Book:**
```bash
./retrieve_kraken_live_data_level2 -p "BTC/USD" --show-top
```
Output:
```
[BTC/USD] Bid: $123900.0 (1.5) | Ask: $123901.0 (1.2) | Spread: $1.0
[BTC/USD] Bid: $123900.0 (1.8) | Ask: $123901.0 (1.2) | Spread: $1.0
```

**Full Order Book Display:**
```bash
./retrieve_kraken_live_data_level2 -p "BTC/USD" --show-book
```
Output:
```
┌─── BTC/USD Order Book (Depth: 10) ───┐
│ Bids              | Asks              │
│ $123900.0 [1.5]   | [1.2] $123901.0   │
│ $123899.5 [2.3]   | [0.8] $123901.5   │
│ ...                                    │
└────────────────────────────────────────┘
```
**Note:** `--show-book` only works with single pair (too complex for multiple)

**Combine Flags:**
```bash
# Updates + top of book
./retrieve_kraken_live_data_level2 -p "BTC/USD" -v --show-top

# Everything (single pair only)
./retrieve_kraken_live_data_level2 -p "BTC/USD" -v --show-top --show-book
```

**Performance Design:**
- Default: Just increment counters (minimal CPU)
- `--show-updates`: Parse message to show details
- `--show-top`: Extract and display best bid/ask
- `--show-book`: Maintain full book state in memory for display

Each flag only adds computation when explicitly requested.

---

### Question 6: Input Methods

**Question:** How should users specify which pairs to monitor?

**Decision:** ✅ **Same as Level 1 (using cli_utils library)**

**Rationale:**
- Consistent user experience across tools
- Already implemented and tested
- Supports all three input methods

**Input Methods:**

**1. Direct List (comma-separated):**
```bash
./retrieve_kraken_live_data_level2 -p "BTC/USD,ETH/USD,SOL/USD"
```

**2. Text File (one pair per line):**
```bash
# All lines
./retrieve_kraken_live_data_level2 -p kraken_tickers.txt

# First 10 lines
./retrieve_kraken_live_data_level2 -p kraken_tickers.txt:10
```

**3. CSV File (specify column):**
```bash
# All rows
./retrieve_kraken_live_data_level2 -p kraken_usd_volume.csv:pair

# First 10 rows
./retrieve_kraken_live_data_level2 -p kraken_usd_volume.csv:pair:10
```

**Implementation:**
- Reuse `cli::InputParser` from cli_utils library
- Automatic format detection
- Same error messages and validation

---

### Question 7: Additional Configuration Options

#### A) Default Output Filename

**Question:** What should the default output filename be?

**Decision:** ✅ **`kraken_orderbook.jsonl`**

**Rationale:**
- Clear, descriptive name
- Consistent with data type (orderbook)
- `.jsonl` extension indicates format

**Implementation:**
```bash
# Uses default filename
./retrieve_kraken_live_data_level2 -p "BTC/USD"
# Creates: kraken_orderbook.jsonl

# Custom filename
./retrieve_kraken_live_data_level2 -p "BTC/USD" -o my_data.jsonl
```

#### B) Snapshot Parameter

**Question:** Should we make Kraken's `snapshot` parameter configurable?

**Options:**
- `snapshot=true`: Receive initial full book state (default)
- `snapshot=false`: Only receive updates (no initial state)

**Decision:** ✅ **Always use `snapshot=true` (not configurable)**

**Rationale:**
- Initial snapshot is essential for replay/analysis
- Without snapshot, you don't know the starting state
- Users may not understand the importance
- Simpler interface (one less option)
- Matches the "capture everything" philosophy

#### C) Symbol Name Mapping

**Question:** Handle Kraken's API vs WebSocket name differences?

**Example:** Kraken API uses "XBT/USD" but WebSocket uses "BTC/USD" in some cases.

**Decision:** ✅ **Automatic symbol mapping (reuse Level 1 logic)**

**Rationale:**
- Consistent with Level 1 tool
- Users shouldn't worry about name differences
- Already implemented and tested

**Implementation:**
- Reuse symbol mapping from Level 1
- Transparent to user
- Works with all input methods

#### D) Multiple Output Files

**Question:** Save all symbols to one file or separate files per symbol?

**Options:**

**Single File (default):**
```
kraken_orderbook.jsonl  # Contains all symbols interleaved
```
- Pros: Simple, chronological order
- Cons: Harder to process per-symbol

**Separate Files:**
```
kraken_orderbook_BTC_USD.jsonl
kraken_orderbook_ETH_USD.jsonl
kraken_orderbook_SOL_USD.jsonl
```
- Pros: Easy to process per-symbol
- Cons: More file handles, more complexity

**Decision:** ✅ **Single file default + `--separate-files` flag**

**Rationale:**
- Single file preserves chronological order (useful for correlation analysis)
- Separate files easier for per-symbol processing
- Give users the choice

**Implementation:**
```bash
# Default: single file
./retrieve_kraken_live_data_level2 -p "BTC/USD,ETH/USD" -o data.jsonl
# Creates: data.jsonl (all symbols)

# Separate files per symbol
./retrieve_kraken_live_data_level2 -p "BTC/USD,ETH/USD" --separate-files
# Creates:
# kraken_orderbook_BTC_USD.jsonl
# kraken_orderbook_ETH_USD.jsonl

# Separate files with custom prefix
./retrieve_kraken_live_data_level2 -p "BTC/USD,ETH/USD" -o mydata --separate-files
# Creates:
# mydata_BTC_USD.jsonl
# mydata_ETH_USD.jsonl
```

---

## Final Design Specification

### Tool Name
`retrieve_kraken_live_data_level2`

### Command-Line Interface

```
Usage: retrieve_kraken_live_data_level2 [OPTIONS]

Retrieve real-time Level 2 order book data from Kraken

Required Arguments:
  -p, --pairs <SPEC>          Pairs specification
                              Formats:
                                - Direct list: "BTC/USD,ETH/USD"
                                - Text file: "file.txt" or "file.txt:10"
                                - CSV file: "file.csv:column" or "file.csv:column:10"

Optional Arguments:
  -d, --depth <NUM>           Order book depth (10, 25, 100, 500, 1000)
                              Default: 10

  -o, --output <FILE>         Output file path
                              Default: kraken_orderbook.jsonl

  --separate-files            Create separate output file per symbol
                              Default: single file for all symbols

  --skip-validation           Skip checksum validation (faster)
                              Default: validate with warnings

Display Options:
  -v, --show-updates          Show update details (snapshot/update counts)
  --show-top                  Show top-of-book (best bid/ask and spread)
  --show-book                 Show full order book display (single pair only)
                              Default: minimal status counters

  -h, --help                  Show this help message

Examples:
  # Minimal capture (fastest)
  ./retrieve_kraken_live_data_level2 -p "BTC/USD,ETH/USD"

  # With depth and top-of-book display
  ./retrieve_kraken_live_data_level2 -p "BTC/USD" -d 25 --show-top

  # From text file, separate output files, no validation
  ./retrieve_kraken_live_data_level2 -p tickers.txt:5 --separate-files --skip-validation

  # Full monitoring (single pair)
  ./retrieve_kraken_live_data_level2 -p "BTC/USD" -d 10 --show-book -v --show-top

  # From CSV, depth 100, custom output
  ./retrieve_kraken_live_data_level2 -p volume.csv:pair:10 -d 100 -o deep_book.jsonl
```

### Output Format

**JSON Lines (.jsonl)** - One JSON object per line

**Message Structure:**
```json
{
  "timestamp": "2025-10-03T14:00:00.123456Z",
  "channel": "book",
  "type": "snapshot",
  "data": {
    "symbol": "BTC/USD",
    "bids": [[123900.0, 1.5], [123899.5, 2.3], ...],
    "asks": [[123901.0, 1.2], [123901.5, 0.8], ...],
    "checksum": 1234567890
  }
}
```

**Update Message:**
```json
{
  "timestamp": "2025-10-03T14:00:00.456789Z",
  "channel": "book",
  "type": "update",
  "data": {
    "symbol": "BTC/USD",
    "bids": [[123900.0, 1.8]],
    "asks": [[123902.0, 0.0]],
    "checksum": 1234567891
  }
}
```

**Note:** In updates, quantity of 0.0 means remove that price level.

### Architecture & Implementation

**Based On:**
- Level 1 tool architecture (`retrieve_kraken_live_data_level1`)
- cli_utils library (argument parsing, input parsing)
- Kraken WebSocket client library (simdjson version)

**New Components Needed:**

1. **OrderBookRecord Structure**
```cpp
struct OrderBookRecord {
    std::string timestamp;
    std::string symbol;
    std::string type;  // "snapshot" or "update"
    std::vector<PriceLevel> bids;
    std::vector<PriceLevel> asks;
    uint32_t checksum;
};

struct PriceLevel {
    double price;
    double quantity;
};
```

2. **Checksum Validator**
```cpp
class ChecksumValidator {
    static uint32_t calculate_crc32(const OrderBookData& data);
    static bool validate(const OrderBookRecord& record);
};
```

3. **Order Book Display**
```cpp
class OrderBookDisplay {
    void show_minimal(const std::map<std::string, Stats>& stats);
    void show_updates(const OrderBookRecord& record);
    void show_top(const OrderBookRecord& record);
    void show_book(const OrderBookRecord& record);  // single pair only
};
```

4. **JSON Lines Writer**
```cpp
class JsonLinesWriter {
    void write_record(const OrderBookRecord& record);
    void flush();
};
```

### Code Reuse

**From Level 1:**
- ✅ CLI argument parsing (cli_utils)
- ✅ Input parsing (direct, text, CSV)
- ✅ WebSocket connection handling
- ✅ Signal handling (Ctrl+C)
- ✅ Condition variable pattern
- ✅ Symbol name mapping

**New for Level 2:**
- Order book data structures
- Checksum validation
- JSON Lines output
- Order book display logic
- Multiple output file handling

### Performance Considerations

**Display Tiers (by overhead):**

| Flag | CPU | Memory | Description |
|------|-----|--------|-------------|
| Default | Minimal | Minimal | Just counters |
| `-v` | Low | Minimal | Parse to count changes |
| `--show-top` | Low | Minimal | Extract best bid/ask |
| `--show-book` | Medium | Medium | Maintain book state |

**Recommendations:**
- Many pairs (10+): Use default or `-v`
- Few pairs (1-5): Safe to use `--show-top`
- Single pair: Can use `--show-book`

### File Size Estimates

**For BTC/USD at depth 10:**
- Snapshot: ~500 bytes
- Update: ~100-300 bytes (varies by changes)
- Updates per second: 1-20 (varies by volatility)

**Storage per hour:**
- Low volatility: ~5-10 MB
- Medium volatility: ~20-50 MB
- High volatility: ~50-100 MB

**For 10 pairs at depth 10:**
- Expected: 50-500 MB per hour

### Testing Plan

**Test Cases:**

1. **Single pair, default depth**
```bash
./retrieve_kraken_live_data_level2 -p "BTC/USD"
```

2. **Multiple pairs, high depth**
```bash
./retrieve_kraken_live_data_level2 -p "BTC/USD,ETH/USD,SOL/USD" -d 100
```

3. **From text file with display options**
```bash
./retrieve_kraken_live_data_level2 -p tickers.txt:5 -v --show-top
```

4. **Separate files mode**
```bash
./retrieve_kraken_live_data_level2 -p "BTC/USD,ETH/USD" --separate-files
```

5. **Skip validation mode**
```bash
./retrieve_kraken_live_data_level2 -p "BTC/USD" --skip-validation
```

6. **Full order book display (single pair)**
```bash
./retrieve_kraken_live_data_level2 -p "BTC/USD" --show-book
```

## Future Tools

### Tool 2: `process_orderbook_snapshots`

**Purpose:** Process raw .jsonl files to create periodic snapshots

**Design (Future):**
```bash
./process_orderbook_snapshots -i raw_data.jsonl --interval 1s -o snapshots_1s.csv
./process_orderbook_snapshots -i raw_data.jsonl --interval 5s -o snapshots_5s.csv
```

**Features:**
- Read .jsonl files
- Rebuild order book state by applying updates
- Sample at specified intervals (1s, 5s, 1m, etc.)
- Output to CSV format for analysis
- Calculate derived metrics (spread, imbalance, depth)

**Output Columns:**
```
timestamp,symbol,best_bid,best_bid_qty,best_ask,best_ask_qty,spread,mid_price,
bid_volume_top10,ask_volume_top10,imbalance,depth_10_bps,depth_25_bps,...
```

### Tool 3: `orderbook_replay`

**Purpose:** Replay order book from saved data

**Design (Future):**
```bash
./orderbook_replay -i raw_data.jsonl --speed 1.0 --symbol BTC/USD
```

**Features:**
- Visual order book display
- Speed control (0.5x, 1x, 2x, 10x)
- Pause/resume
- Jump to timestamp
- Metrics overlay

## Related Documentation

- **Level 1 Tool Design:** `cpp/examples/retrieve_kraken_live_data_level1.cpp`
- **CLI Utils Library:** `cpp/docs/CLI_UTILS_QUICK_REFERENCE.md`
- **Project Structure:** `cpp/docs/PROJECT_STRUCTURE.md`
- **Kraken API Docs:** https://docs.kraken.com/api/docs/websocket-v2/book

## Implementation Status

- [ ] Design documented (this file)
- [ ] Order book data structures
- [ ] Checksum validation
- [ ] JSON Lines writer
- [ ] WebSocket subscription for book channel
- [ ] Display logic (minimal, updates, top, full)
- [ ] Separate files mode
- [ ] Testing
- [ ] Documentation

## Notes

- This design prioritizes separation of concerns (capture vs processing)
- Raw data is preserved for maximum flexibility
- Performance is tunable via display flags
- Consistent with Level 1 tool for ease of use
- Foundation for future analysis tools

---

**Document Version:** 1.0
**Last Updated:** October 3, 2025
**Status:** Design Complete, Ready for Implementation
