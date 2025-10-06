# Level 3 Order Book - Implementation Summary

**Version:** 1.0
**Date:** October 3, 2025
**Status:** Core Libraries Complete, Tools In Progress

---

## Overview

Implementation of Level 3 order book tools for Kraken WebSocket v2 API. Level 3 provides **individual order granularity** (order_id, limit_price, order_qty, timestamp) versus Level 2's aggregated price levels.

## Architecture

**Two-Tool Approach** (consistent with Level 2):
1. **`retrieve_kraken_live_data_level3`** - Capture raw order-level data
2. **`process_level3_snapshots`** - Analyze and generate metrics

---

## Implementation Status

### ✅ COMPLETED - Core Libraries

#### 1. Data Structures (`level3_common.hpp/cpp`)

**Structures:**
```cpp
struct Level3Order {
    std::string order_id;
    double limit_price;
    double order_qty;
    std::string timestamp;  // RFC3339
    std::string event;      // "add"/"modify"/"delete" for updates
};

struct Level3Record {
    std::string timestamp;
    std::string symbol;
    std::string type;  // "snapshot" or "update"
    std::vector<Level3Order> bids;
    std::vector<Level3Order> asks;
    uint32_t checksum;
};

struct Level3Stats {
    int snapshot_count;
    int update_count;
    int add_events;
    int modify_events;
    int delete_events;
    int bid_order_count;
    int ask_order_count;
    double best_bid;
    double best_ask;
    double spread;
};
```

**Display Utilities:**
- `show_minimal()` - Counter display (default)
- `show_event_counts()` - Add/modify/delete stats
- `show_top_of_book()` - Best bid/ask with order details
- `show_order_event()` - Live order event feed
- `update_stats()` - Statistics tracking

**File:** `cpp/lib/level3_common.{hpp,cpp}` (240 lines)

---

#### 2. JSON Lines Writer (`level3_jsonl_writer.hpp/cpp`)

**Classes:**
- `Level3JsonLinesWriter` - Single-file output
- `MultiFileLevel3JsonLinesWriter` - Separate file per symbol

**Features:**
- Full-precision order data preservation
- Proper JSON escaping
- Event field for updates
- Automatic file management per symbol

**Output Format Example:**
```json
{
  "timestamp": "2025-10-03 10:00:00.123",
  "channel": "level3",
  "type": "snapshot",
  "data": {
    "symbol": "BTC/USD",
    "bids": [{
      "order_id": "OABC123",
      "limit_price": 122850.00,
      "order_qty": 0.105,
      "timestamp": "2025-10-03T10:00:00.123Z"
    }],
    "asks": [...],
    "checksum": 123456789
  }
}
```

**File:** `cpp/lib/level3_jsonl_writer.{hpp,cpp}` (350 lines)

---

#### 3. WebSocket Client (`kraken_level3_client.hpp/cpp`)

**Key Features:**
- **Authentication Support** (Level 3 requirement)
  - Priority: `--token` > `--token-file` > `KRAKEN_WS_TOKEN` env var
  - Secure token file reading
  - Environment variable support
- **Order-level Message Parsing**
  - Snapshots with full order details
  - Updates with event types (add/modify/delete)
- **Error Handling**
  - Clear authentication errors
  - Server-side depth validation
  - Subscription failure feedback
- **Statistics Tracking**
  - Order counts per side
  - Event type counters
  - Real-time best bid/ask

**Authentication Methods:**
```cpp
// Method 1: Direct token (testing)
client.set_token("YOUR_TOKEN");

// Method 2: Token file (recommended)
client.set_token_from_file("~/.kraken/ws_token");

// Method 3: Environment variable (default)
export KRAKEN_WS_TOKEN="YOUR_TOKEN"
client.set_token_from_env();
```

**Subscription Format:**
```json
{
  "method": "subscribe",
  "params": {
    "channel": "level3",
    "symbol": ["BTC/USD", "ETH/USD"],
    "depth": 10,
    "snapshot": true,
    "token": "AUTH_TOKEN"
  }
}
```

**File:** `cpp/lib/kraken_level3_client.{hpp,cpp}` (550 lines)

---

### ⏳ IN PROGRESS - Main Tools

#### 4. Capture Tool (`retrieve_kraken_live_data_level3.cpp`)

**Status:** Ready to implement

**Command-Line Interface:**
```bash
./retrieve_kraken_live_data_level3 -p "BTC/USD" [OPTIONS]

Required:
  -p, --pairs <SPEC>          Pairs specification

Authentication (priority order):
  --token <TOKEN>             Direct token (highest priority)
  --token-file <FILE>         Token file
  KRAKEN_WS_TOKEN env var     Default

Optional:
  -d, --depth <NUM>           Depth: 10, 100, 1000 (default: 10)
  -o, --output <FILE>         Output file (default: kraken_level3.jsonl)
  --separate-files            Separate file per symbol

Display:
  -v, --show-events           Show event counts
  --show-top                  Show best bid/ask with orders
  --show-orders               Live order feed (verbose)
```

**Pattern:** Similar to `retrieve_kraken_live_data_level2.cpp`
- Use cli_utils for argument parsing
- Use KrakenLevel3Client for WebSocket
- Use Level3JsonLinesWriter for output
- Handle authentication token setup
- Display modes via callback

---

#### 5. State Rebuilder (`level3_state.hpp/cpp`)

**Status:** Ready to implement

**Dual Indexing Design:**
```cpp
class Level3OrderBookState {
private:
    // Fast order lookup for updates/deletes
    std::map<std::string, Order*> orders_by_id_;

    // Fast price-level iteration for metrics
    std::map<double, std::vector<Order*>, std::greater<double>> bids_by_price_;
    std::map<double, std::vector<Order*>> asks_by_price_;

    // Event tracking
    int add_count_;
    int modify_count_;
    int delete_count_;
};
```

**Operations:**
- `apply_snapshot()` - Initialize from snapshot
- `apply_update()` - Handle add/modify/delete events
- `get_best_bid/ask()` - O(1) access to best prices
- `get_order_count_at_price()` - Count orders at level
- `get_avg_order_size()` - Calculate average sizes
- `calculate_flow_metrics()` - Event rates

---

#### 6. Processing Tool (`process_level3_snapshots.cpp`)

**Status:** Ready to implement

**Command-Line Interface:**
```bash
./process_level3_snapshots -i level3_raw.jsonl --interval 1s -o snapshots.csv

Required:
  -i, --input <FILE>          Input .jsonl file
  --interval <TIME>           Sampling interval (1s, 5s, 1m, 1h)

Optional:
  -o, --output <FILE>         Output CSV (default: level3_snapshots.csv)
  --separate-files            Separate CSV per symbol
  --symbol <LIST>             Filter symbols
  --skip-validation           Skip checksum validation
```

**Output Metrics:**
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

**Pattern:** Similar to `process_orderbook_snapshots.cpp`
- Use Level3OrderBookState for state management
- Calculate all Level 2 metrics (aggregated from orders)
- Add Level 3-specific metrics (order counts, flow)
- Adaptive precision CSV output

---

### 📋 PENDING - Build & Test

#### 7. CMakeLists.txt Integration

**Libraries to add:**
```cmake
add_library(level3_common STATIC lib/level3_common.cpp)
add_library(level3_jsonl_writer STATIC lib/level3_jsonl_writer.cpp)
```

**Tools to add:**
```cmake
add_executable(retrieve_kraken_live_data_level3
    examples/retrieve_kraken_live_data_level3.cpp)
target_link_libraries(retrieve_kraken_live_data_level3
    kraken_common cli_utils level3_common level3_jsonl_writer
    simdjson ${OPENSSL_LIBRARIES} ${Boost_LIBRARIES} pthread)

add_executable(process_level3_snapshots
    examples/process_level3_snapshots.cpp)
target_link_libraries(process_level3_snapshots
    cli_utils level3_common level3_state snapshot_csv_writer simdjson)
```

#### 8. Testing Plan

**Phase 1: Authentication**
- ✓ Test --token parameter
- ✓ Test --token-file reading
- ✓ Test KRAKEN_WS_TOKEN env var
- ✓ Verify priority order
- ✓ Test missing token error handling

**Phase 2: Capture Tool**
- ✓ Single symbol, depth=10
- ✓ Multiple symbols
- ✓ depth=100 and depth=1000
- ✓ Separate files mode
- ✓ All display modes (minimal, events, top, orders)

**Phase 3: Processing Tool**
- ✓ Basic metrics calculation
- ✓ Order count metrics
- ✓ Flow metrics (add/modify/delete rates)
- ✓ Symbol filtering
- ✓ Interval sampling (1s, 5s, 1m)

**Phase 4: Integration**
- ✓ Capture → Process pipeline
- ✓ Compare Level 3 aggregated metrics with Level 2
- ✓ Long-running stability test
- ✓ High-depth (1000) stress test

---

## Key Differences: Level 2 vs Level 3

| Feature | Level 2 | Level 3 |
|---------|---------|---------|
| **Granularity** | Aggregated price levels | Individual orders |
| **Data Structure** | `{price, qty}` | `{order_id, price, qty, timestamp}` |
| **Updates** | Price level changes | Order events (add/modify/delete) |
| **Authentication** | Not required | **Required (token)** |
| **Channel** | `book` | `level3` |
| **Depths** | 10, 25, 100, 500, 1000 | 10, 100, 1000 |
| **Message Size** | Smaller | Larger (more data per level) |
| **State Management** | Simple map by price | **Dual indexing** (by ID and price) |
| **Metrics** | Basic market metrics | + Order flow, queue dynamics |

---

## File Structure

```
cpp/
├── lib/
│   ├── level3_common.{hpp,cpp}          ✅ COMPLETE
│   ├── level3_jsonl_writer.{hpp,cpp}    ✅ COMPLETE
│   ├── kraken_level3_client.{hpp,cpp}   ✅ COMPLETE
│   ├── level3_state.{hpp,cpp}           ⏳ PENDING
│   └── ...
├── examples/
│   ├── retrieve_kraken_live_data_level3.cpp  ⏳ PENDING
│   ├── process_level3_snapshots.cpp          ⏳ PENDING
│   └── ...
├── docs/
│   ├── LEVEL3_ORDERBOOK_DESIGN.md        ✅ COMPLETE
│   └── ...
└── CMakeLists.txt                         ⏳ PENDING (updates)
```

---

## Code Statistics

**Implemented (Core Libraries):**
- Lines of code: ~1,140 lines
- Files created: 6 files (3 headers, 3 implementations)
- Data structures: 3 main structs
- Classes: 4 classes
- Functions: ~30 functions

**Remaining (Tools):**
- Estimated: ~800 lines
- Files: 4 files (2 tools + state rebuilder)
- Expected total: ~1,940 lines for complete Level 3 system

---

## Usage Example (When Complete)

### Step 1: Set Authentication
```bash
# Option A: Environment variable (recommended)
export KRAKEN_WS_TOKEN="your_websocket_token_here"

# Option B: Token file
echo "your_websocket_token_here" > ~/.kraken/ws_token
chmod 600 ~/.kraken/ws_token
```

### Step 2: Capture Raw Data
```bash
# Minimal capture
./retrieve_kraken_live_data_level3 -p "BTC/USD"

# With display
./retrieve_kraken_live_data_level3 -p "BTC/USD,ETH/USD" -d 100 -v --show-top

# Live order feed
./retrieve_kraken_live_data_level3 -p "BTC/USD" --show-orders
```

### Step 3: Process and Analyze
```bash
# Generate 1-second snapshots
./process_level3_snapshots -i kraken_level3.jsonl --interval 1s -o snapshots_1s.csv

# Generate 5-second snapshots for specific symbol
./process_level3_snapshots -i kraken_level3.jsonl --interval 5s --symbol BTC/USD -o btc_5s.csv

# Separate files per symbol
./process_level3_snapshots -i kraken_level3.jsonl --interval 1s --separate-files
```

### Step 4: Analyze in Python/R
```python
import pandas as pd

# Load snapshots
df = pd.read_csv('snapshots_1s.csv')

# Analyze order flow
print(df[['timestamp', 'order_arrival_rate', 'order_cancel_rate']].head())

# Analyze imbalance
print(df[['timestamp', 'imbalance', 'bid_order_count', 'ask_order_count']].head())
```

---

## Next Steps

1. **Implement capture tool** - `retrieve_kraken_live_data_level3.cpp`
2. **Implement state rebuilder** - `level3_state.{hpp,cpp}`
3. **Implement processing tool** - `process_level3_snapshots.cpp`
4. **Update CMakeLists.txt** - Add all Level 3 targets
5. **Test end-to-end** - Capture → Process → Analyze workflow
6. **Document authentication** - Add token setup guide
7. **Update project docs** - Add Level 3 to main documentation index

---

## Related Documentation

- **Design Document:** `cpp/docs/LEVEL3_ORDERBOOK_DESIGN.md`
- **Level 2 Design:** `cpp/docs/LEVEL2_ORDERBOOK_DESIGN.md`
- **CLI Utils:** `cpp/docs/CLI_UTILS_QUICK_REFERENCE.md`
- **Kraken API:** https://docs.kraken.com/api/docs/websocket-v2/level3

---

**Status:** Core libraries complete, ready to build main tools
**Last Updated:** October 3, 2025
**Completion:** ~60% (core infrastructure done, tools remaining)
