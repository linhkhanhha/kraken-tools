# Level 3 Order Book Tool - Design Document

**Version:** 1.0
**Date:** October 3, 2025
**Status:** Design Complete, Ready for Implementation

## Overview

Level 3 order book data provides **individual order granularity** from Kraken WebSocket v2. Unlike Level 2 which shows aggregated price levels, Level 3 shows each individual order with its unique `order_id`, allowing tracking of order queue dynamics and flow.

## Design Decisions Summary

### ✅ Decision 1: Tool Architecture
- **Two-tool approach** (consistent with Level 2)
  - Tool 1: `retrieve_kraken_live_data_level3` (capture raw data)
  - Tool 2: `process_level3_snapshots` (analysis)
- **Rationale:** Separation of concerns, flexibility, consistency

### ✅ Decision 2: Authentication
- **Support all three methods with priority:**
  1. `--token <token>` (highest priority)
  2. `--token-file <path>` (medium priority)
  3. `KRAKEN_WS_TOKEN` environment variable (default)
- **Rationale:** Security by default, flexibility for testing

### ✅ Decision 3: Depth Validation
- **Server-side validation**
- Valid depths: 10, 100, 1000
- Tool shows immediate error feedback when server rejects
- **Rationale:** Future-proof, simpler code

### ✅ Decision 4: Output Format
- **JSON Lines (.jsonl)** - same as Level 2
- Full field names (not compressed)
- Store individual order details
- **Rationale:** Consistency, readability, tooling compatibility

### ✅ Decision 5: Display Options
- **Hybrid tiered approach:**
  - Default: Minimal counters
  - `-v`: Event counts (add/modify/delete)
  - `--show-top`: Best bid/ask with order details
  - `--show-orders`: Live order event feed (verbose)
- **Rationale:** Performance first, verbosity on demand

### ✅ Decision 6: Processing Metrics
- **Comprehensive metrics:**
  - All Level 2 metrics (spread, volume, imbalance, depth)
  - Order counts: total orders, orders at best price
  - Order sizes: average order size on each side
  - Order flow: add/modify/delete event counts
  - Flow rates: order arrival rate, cancel rate
- **Rationale:** Level 3 is rich data, provide full analysis

### ✅ Decision 7: State Management
- **Dual indexing:**
  - `std::map<order_id, Order*>` for fast order lookup
  - `std::map<price, std::vector<Order*>>` for fast price iteration
- **Rationale:** Optimize both update operations and aggregation

---

## Tool 1: retrieve_kraken_live_data_level3

### Command-Line Interface

```
Usage: retrieve_kraken_live_data_level3 [OPTIONS]

Retrieve real-time Level 3 order book data from Kraken

Required Arguments:
  -p, --pairs <SPEC>          Pairs specification
                              Formats:
                                - Direct list: "BTC/USD,ETH/USD"
                                - Text file: "file.txt" or "file.txt:10"
                                - CSV file: "file.csv:column" or "file.csv:column:10"

Authentication (priority order):
  --token <TOKEN>             Authentication token (highest priority)
  --token-file <FILE>         File containing token
  (or) KRAKEN_WS_TOKEN        Environment variable (default)

Optional Arguments:
  -d, --depth <NUM>           Order book depth (10, 100, 1000)
                              Default: 10

  -o, --output <FILE>         Output file path
                              Default: kraken_level3.jsonl

  --separate-files            Create separate output file per symbol
                              Default: single file for all symbols

Display Options:
  -v, --show-events           Show event counts (add/modify/delete)
  --show-top                  Show best bid/ask with order details
  --show-orders               Show live order event feed (verbose)
                              Default: minimal counters

  -h, --help                  Show this help message
```

### Usage Examples

```bash
# Set token via environment variable
export KRAKEN_WS_TOKEN="your_token_here"

# Minimal capture (fastest)
./retrieve_kraken_live_data_level3 -p "BTC/USD"

# With depth and event display
./retrieve_kraken_live_data_level3 -p "BTC/USD" -d 100 -v

# Using token file
./retrieve_kraken_live_data_level3 -p "BTC/USD,ETH/USD" --token-file ~/.kraken/ws_token

# Separate files per symbol
./retrieve_kraken_live_data_level3 -p tickers.txt:10 --separate-files

# Live order feed (verbose)
./retrieve_kraken_live_data_level3 -p "BTC/USD" --show-orders

# Using direct token (for testing)
./retrieve_kraken_live_data_level3 -p "BTC/USD" --token "TOKEN_HERE"
```

### Data Structures

**Level3Order:**
```cpp
struct Level3Order {
    std::string order_id;
    double limit_price;
    double order_qty;
    std::string timestamp;  // RFC3339 format
};
```

**Level3Record:**
```cpp
struct Level3Record {
    std::string timestamp;
    std::string symbol;
    std::string type;  // "snapshot" or "update"
    std::vector<Level3Order> bids;
    std::vector<Level3Order> asks;
    uint32_t checksum;

    // For updates
    std::string event;  // "add", "modify", "delete"
};
```

### Output Format (JSON Lines)

**Snapshot:**
```json
{
  "timestamp": "2025-10-03 10:00:00.123",
  "channel": "level3",
  "type": "snapshot",
  "data": {
    "symbol": "BTC/USD",
    "bids": [
      {
        "order_id": "OABC123",
        "limit_price": 122850.00,
        "order_qty": 0.105,
        "timestamp": "2025-10-03T10:00:00.123Z"
      }
    ],
    "asks": [...],
    "checksum": 123456789
  }
}
```

**Update:**
```json
{
  "timestamp": "2025-10-03 10:00:01.456",
  "channel": "level3",
  "type": "update",
  "data": {
    "symbol": "BTC/USD",
    "bids": [
      {
        "event": "add",
        "order_id": "OXYZ789",
        "limit_price": 122849.50,
        "order_qty": 0.250,
        "timestamp": "2025-10-03T10:00:01.456Z"
      }
    ],
    "asks": [],
    "checksum": 123456790
  }
}
```

---

## Tool 2: process_level3_snapshots

### Command-Line Interface

```
Usage: process_level3_snapshots [OPTIONS]

Process Level 3 order book data to create periodic snapshots with metrics

Required Arguments:
  -i, --input <FILE>          Input .jsonl file from retrieve_kraken_live_data_level3
  --interval <TIME>           Sampling interval (e.g., 1s, 5s, 1m, 1h)

Optional Arguments:
  -o, --output <FILE>         Output CSV filename
                              Default: level3_snapshots.csv

  --separate-files            Create separate output file per symbol
  --symbol <LIST>             Filter to specific symbol(s)
  --skip-validation           Skip checksum validation (faster)

  -h, --help                  Show this help message
```

### Usage Examples

```bash
# Basic processing - 1 second snapshots
./process_level3_snapshots -i level3_raw.jsonl --interval 1s -o snapshots.csv

# 5 second snapshots with symbol filter
./process_level3_snapshots -i level3_raw.jsonl --interval 5s --symbol BTC/USD -o btc.csv

# Separate files per symbol
./process_level3_snapshots -i level3_raw.jsonl --interval 1s --separate-files

# 1 minute snapshots, skip validation for speed
./process_level3_snapshots -i level3_raw.jsonl --interval 1m --skip-validation
```

### Output Metrics (CSV)

```csv
timestamp,symbol,
best_bid,best_bid_qty,best_ask,best_ask_qty,spread,spread_bps,mid_price,
bid_volume_top10,ask_volume_top10,imbalance,
depth_10_bps,depth_25_bps,depth_50_bps,
bid_order_count,ask_order_count,
bid_orders_at_best,ask_orders_at_best,
avg_bid_order_size,avg_ask_order_size,
add_events,modify_events,delete_events,
order_arrival_rate,order_cancel_rate
```

**Metric Definitions:**

**Basic Metrics (from Level 2):**
- `best_bid/ask` - Best price on each side
- `spread` - Absolute spread
- `spread_bps` - Spread in basis points
- `mid_price` - Mid-market price

**Volume Metrics (from Level 2):**
- `bid/ask_volume_top10` - Total volume in top 10 levels
- `imbalance` - Volume imbalance ratio

**Depth Metrics (from Level 2):**
- `depth_X_bps` - Volume within X basis points of mid

**Order Count Metrics (Level 3):**
- `bid/ask_order_count` - Total orders on each side
- `bid/ask_orders_at_best` - Number of orders at best price
- `avg_bid/ask_order_size` - Average order size

**Order Flow Metrics (Level 3):**
- `add_events` - New orders added in interval
- `modify_events` - Order modifications in interval
- `delete_events` - Order cancellations in interval
- `order_arrival_rate` - Orders per second (adds)
- `order_cancel_rate` - Cancellations per second (deletes)

### State Management

**Dual Indexing:**
```cpp
class Level3OrderBookState {
private:
    // Fast order lookup for updates/deletes
    std::map<std::string, Order*> orders_by_id_;

    // Fast price-level iteration for metrics
    std::map<double, std::vector<Order*>, std::greater<double>> bids_by_price_;
    std::map<double, std::vector<Order*>> asks_by_price_;

    // Event tracking for flow metrics
    int add_count_;
    int modify_count_;
    int delete_count_;
};
```

---

## Key Differences: Level 2 vs Level 3

| Feature | Level 2 | Level 3 |
|---------|---------|---------|
| **Granularity** | Aggregated price levels | Individual orders |
| **Data per level** | `{price, qty}` | `{order_id, price, qty, timestamp}` |
| **Update format** | Price level changes | Order events (add/modify/delete) |
| **Authentication** | Not required | Required (token) |
| **Channel name** | `book` | `level3` |
| **Depth options** | 10, 25, 100, 500, 1000 | 10, 100, 1000 |
| **Use case** | Market overview, spreads | Order flow, queue dynamics |
| **Data volume** | Lower | Higher |
| **Processing** | Simpler aggregation | Complex order tracking |

---

## Implementation Notes

### Authentication Flow
1. Check `--token` parameter first
2. If not provided, check `--token-file`
3. If not provided, check `KRAKEN_WS_TOKEN` env var
4. If none provided, show error with instructions

### Error Handling
- Invalid depth: Show server error immediately
- Auth failure: Clear error message, instructions to set token
- Connection loss: Attempt reconnection with backoff
- Checksum mismatch: Warn but continue (optional --strict mode)

### Performance Considerations
- Dual indexing adds memory overhead (~2x)
- Worth it for O(log n) order lookups and O(1) price iteration
- Expected: ~10K orders for depth=1000, manageable in memory

### Testing Strategy
1. Single symbol, depth=10, short duration
2. Multiple symbols with separate files
3. High-depth (1000) stress test
4. Long-running stability test
5. Verify all metrics calculations
6. Compare Level 2 aggregated metrics with Level 3 aggregated

---

## Future Enhancements

### Tool 3: level3_to_level2 Converter
```bash
./level3_to_level2 -i level3_raw.jsonl -o level2.jsonl
# Aggregate Level 3 orders to Level 2 price levels
# Useful for validation and comparison
```

### Tool 4: order_flow_analyzer
```bash
./order_flow_analyzer -i level3_raw.jsonl -o flow_metrics.csv
# Specialized tool for order flow analysis
# Order arrival patterns, queue jumping, order sizes
```

### Tool 5: level3_replay
```bash
./level3_replay -i level3_raw.jsonl --speed 1.0 --symbol BTC/USD
# Visual replay with order-level details
# Show order additions, cancellations, trades
```

### Compression Tool
```bash
./compress_level3 -i level3_raw.jsonl -o level3_compressed.bin
# Binary format with compression
# Useful for long-term storage
```

---

## Related Documentation

- **Level 2 Tool Design:** `cpp/docs/LEVEL2_ORDERBOOK_DESIGN.md`
- **CLI Utils Library:** `cpp/docs/CLI_UTILS_QUICK_REFERENCE.md`
- **Kraken Level 3 API:** https://docs.kraken.com/api/docs/websocket-v2/level3

---

**Document Version:** 1.0
**Last Updated:** October 3, 2025
**Status:** Design Complete, Ready for Implementation
