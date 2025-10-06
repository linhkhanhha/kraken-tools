# Project Structure

## Overview

This C++ implementation provides a comprehensive suite of tools for working with Kraken's WebSocket v2 API, including real-time ticker data (Level 1), aggregated order book data (Level 2), and individual order-level data (Level 3).

The project is organized into reusable libraries, educational examples, and production-ready tools.

## Directory Structure

```
cpp/
├── lib/                                # Reusable Libraries
│   ├── Core Libraries
│   │   ├── kraken_common.{hpp,cpp}                    # Shared utilities (timestamps, utils)
│   │   ├── cli_utils.{hpp,cpp}                        # CLI argument parsing, input parsing
│   │   └── kraken_websocket_client*.{hpp,cpp}         # WebSocket clients (various versions)
│   │
│   ├── Level 1 (Ticker) Libraries
│   │   └── (uses kraken_websocket_client)
│   │
│   ├── Level 2 (Order Book) Libraries
│   │   ├── orderbook_common.{hpp,cpp}                 # Level 2 data structures
│   │   ├── orderbook_state.{hpp,cpp}                  # Order book state rebuilder
│   │   ├── snapshot_csv_writer.{hpp,cpp}              # CSV output with adaptive precision
│   │   └── jsonl_writer.{hpp,cpp}                     # JSON Lines output
│   │
│   └── Level 3 (Order-Level) Libraries
│       ├── level3_common.{hpp,cpp}                    # Level 3 data structures, display
│       ├── level3_state.{hpp,cpp}                     # Dual-indexed state rebuilder
│       ├── level3_csv_writer.{hpp,cpp}                # CSV output for Level 3 metrics
│       ├── level3_jsonl_writer.{hpp,cpp}              # JSON Lines output for orders
│       └── kraken_level3_client.{hpp,cpp}             # Authenticated WebSocket client
│
├── examples/                           # Applications
│   ├── Educational Examples
│   │   ├── example_simple_polling.cpp                 # Simple polling pattern
│   │   ├── example_callback_driven.cpp                # Callback/event-driven pattern
│   │   ├── example_integration.cpp                    # System integration pattern
│   │   ├── example_integration_cond.cpp               # Responsive integration with CVs
│   │   ├── example_simdjson_comparison.cpp            # Performance comparison tool
│   │   ├── example_template_version.cpp               # Template-based client demo
│   │   ├── threading_examples.cpp                     # Threading patterns demo
│   │   └── sleep_vs_condition_variable.cpp            # Sleep vs CV comparison
│   │
│   └── Production Tools ⭐
│       ├── Level 1 (Ticker)
│       │   └── retrieve_kraken_live_data_level1.cpp   # Real-time ticker retriever
│       │
│       ├── Level 2 (Order Book)
│       │   ├── retrieve_kraken_live_data_level2.cpp   # Capture order book data
│       │   └── process_orderbook_snapshots.cpp        # Generate metrics from raw data
│       │
│       └── Level 3 (Order-Level)
│           ├── retrieve_kraken_live_data_level3.cpp   # Capture order-level data
│           └── process_level3_snapshots.cpp           # Generate metrics from orders
│
├── legacy/                             # Legacy Implementations (for reference)
│   ├── query_live_data_v2.cpp                         # Original blocking version
│   ├── query_live_data_v2_refactored.cpp              # Refactored blocking version
│   ├── query_live_data_v2_standalone.cpp              # Original demo
│   └── query_live_data_v2_standalone_refactored.cpp   # Refactored demo
│
├── docs/                               # Documentation
│   ├── Getting Started
│   │   ├── EXAMPLES_README.md                         # ⭐⭐⭐ START HERE
│   │   ├── QUICK_REFERENCE.md                         # API quick reference
│   │   └── README.md                                  # Original setup guide
│   │
│   ├── Architecture & Design
│   │   ├── PROJECT_STRUCTURE.md                       # This file
│   │   ├── BLOCKING_VS_NONBLOCKING.md                 # Design comparison
│   │   ├── REFACTORING.md                             # Refactoring process
│   │   └── SUMMARY.md                                 # Project overview
│   │
│   ├── Order Book Documentation
│   │   ├── LEVEL2_ORDERBOOK_DESIGN.md                 # Level 2 design decisions
│   │   ├── LEVEL3_ORDERBOOK_DESIGN.md                 # Level 3 design decisions
│   │   └── LEVEL3_IMPLEMENTATION_SUMMARY.md           # Level 3 implementation status
│   │
│   ├── CLI Utils Library
│   │   ├── CLI_UTILS_QUICK_REFERENCE.md               # API reference
│   │   ├── CLI_UTILS_FEATURE_SUMMARY.md               # Overview and benefits
│   │   ├── CLI_UTILS_TEXT_FILE_SUPPORT.md             # Text file feature guide
│   │   └── CLI_UTILS_REFACTORING.md                   # Refactoring summary
│   │
│   ├── Performance & Analysis
│   │   ├── JSON_LIBRARY_COMPARISON.md                 # JSON library benchmarks
│   │   ├── SIMDJSON_MIGRATION.md                      # simdjson migration plan
│   │   ├── SIMDJSON_USAGE_GUIDE.md                    # simdjson usage guide
│   │   └── FLOAT_VS_DOUBLE_ANALYSIS.md                # Precision analysis
│   │
│   └── Development
│       ├── DEVELOPMENT_NOTES.md                       # Build issues, troubleshooting
│       └── README_REFACTORED.md                       # Refactored code guide
│
├── build/                              # Build Outputs (generated)
│   ├── Libraries (*.a)
│   ├── Executables
│   └── CMake files
│
├── Dependencies (auto-downloaded)
│   ├── websocketpp/                    # WebSocket library
│   ├── include/nlohmann/json.hpp       # JSON library (nlohmann)
│   └── simdjson/                       # JSON library (simdjson, high-performance)
│
└── Build System
    ├── CMakeLists.txt                  # CMake configuration
    └── Makefile                        # Alternative build system
```

## Component Architecture

### Level 1 (Ticker Data)
```
┌─────────────────────────────────────────────────────┐
│  retrieve_kraken_live_data_level1                   │
│  ┌──────────────────────────────────────────────┐   │
│  │  KrakenWebSocketClient (simdjson)            │   │
│  │  - Subscribe to ticker channel               │   │
│  │  - Parse ticker updates                      │   │
│  │  - Output to CSV                             │   │
│  └──────────────────────────────────────────────┘   │
│                    │                                 │
│                    ▼                                 │
│  ┌──────────────────────────────────────────────┐   │
│  │  kraken_common + cli_utils                   │   │
│  └──────────────────────────────────────────────┘   │
└─────────────────────────────────────────────────────┘
```

### Level 2 (Order Book) - Two-Tool Architecture
```
┌─────────────────────────────────────────────────────┐
│  CAPTURE: retrieve_kraken_live_data_level2          │
│  ┌──────────────────────────────────────────────┐   │
│  │  KrakenBookClient                            │   │
│  │  - Subscribe to book channel                 │   │
│  │  - Receive snapshots & updates               │   │
│  │  - Write to JSON Lines (.jsonl)              │   │
│  └──────────────────────────────────────────────┘   │
│                    │                                 │
│                    ▼ raw data                        │
│                 .jsonl file                          │
│                    │                                 │
│  ──────────────────┴───────────────────────────────  │
│                                                       │
│  PROCESS: process_orderbook_snapshots                │
│  ┌──────────────────────────────────────────────┐   │
│  │  OrderBookState                              │   │
│  │  - Rebuild order book from raw data          │   │
│  │  - Calculate metrics at intervals            │   │
│  │  - Output to CSV                             │   │
│  └──────────────────────────────────────────────┘   │
│                    │                                 │
│                    ▼ metrics                         │
│               snapshots.csv                          │
└─────────────────────────────────────────────────────┘
```

### Level 3 (Order-Level) - Two-Tool Architecture
```
┌─────────────────────────────────────────────────────┐
│  CAPTURE: retrieve_kraken_live_data_level3          │
│  ┌──────────────────────────────────────────────┐   │
│  │  KrakenLevel3Client (authenticated)          │   │
│  │  - Subscribe to level3 channel (token)       │   │
│  │  - Receive order snapshots & events          │   │
│  │  - Write individual orders to JSON Lines     │   │
│  └──────────────────────────────────────────────┘   │
│                    │                                 │
│                    ▼ raw order data                  │
│                 .jsonl file                          │
│                    │                                 │
│  ──────────────────┴───────────────────────────────  │
│                                                       │
│  PROCESS: process_level3_snapshots                   │
│  ┌──────────────────────────────────────────────┐   │
│  │  Level3OrderBookState                        │   │
│  │  - Dual indexing (by ID + by price)          │   │
│  │  - Apply add/modify/delete events            │   │
│  │  - Calculate comprehensive metrics           │   │
│  │  - Output to CSV with order flow stats       │   │
│  └──────────────────────────────────────────────┘   │
│                    │                                 │
│                    ▼ metrics                         │
│           level3_snapshots.csv                       │
└─────────────────────────────────────────────────────┘
```

## File Organization by Purpose

### 1. Core Reusable Libraries

| File | Purpose | Lines |
|------|---------|-------|
| `kraken_common.{hpp,cpp}` | Common utilities (timestamps, utils) | 180 |
| `cli_utils.{hpp,cpp}` | CLI argument parsing, input parsing | 1020 |
| `kraken_websocket_client.{hpp,cpp}` | WebSocket client (nlohmann) | 460 |
| `kraken_websocket_client_simdjson.{hpp,cpp}` | WebSocket client (simdjson) | 490 |

**Total**: ~2150 lines

### 2. Level 2 (Order Book) Libraries

| File | Purpose | Lines |
|------|---------|-------|
| `orderbook_common.{hpp,cpp}` | Data structures, display, parsing | 350 |
| `orderbook_state.{hpp,cpp}` | State rebuilder with CRC32 validation | 380 |
| `snapshot_csv_writer.{hpp,cpp}` | CSV output with adaptive precision | 280 |
| `jsonl_writer.{hpp,cpp}` | JSON Lines writer for raw data | 220 |

**Total**: ~1230 lines

### 3. Level 3 (Order-Level) Libraries

| File | Purpose | Lines |
|------|---------|-------|
| `level3_common.{hpp,cpp}` | Data structures, statistics, display | 240 |
| `level3_state.{hpp,cpp}` | Dual-indexed state rebuilder | 520 |
| `level3_csv_writer.{hpp,cpp}` | CSV output for Level 3 metrics | 280 |
| `level3_jsonl_writer.{hpp,cpp}` | JSON Lines writer for orders | 220 |
| `kraken_level3_client.{hpp,cpp}` | Authenticated WebSocket client | 590 |

**Total**: ~1850 lines

### 4. Educational Examples

| File | Pattern | Purpose | Lines |
|------|---------|---------|-------|
| `example_simple_polling.cpp` | Polling | Simple periodic checking | 80 |
| `example_callback_driven.cpp` | Callbacks | Event-driven | 90 |
| `example_integration.cpp` | Hybrid | Multi-component system | 140 |
| `example_integration_cond.cpp` | Hybrid + CV | Responsive integration | 160 |
| `example_simdjson_comparison.cpp` | Benchmark | Performance comparison | 260 |
| `example_template_version.cpp` | Templates | Template-based client | 120 |
| `threading_examples.cpp` | Educational | Threading patterns | 180 |
| `sleep_vs_condition_variable.cpp` | Educational | Sleep vs CV comparison | 150 |

**Total**: ~1180 lines

### 5. Production Tools ⭐

| File | Purpose | Lines |
|------|---------|-------|
| **Level 1 (Ticker)** |
| `retrieve_kraken_live_data_level1.cpp` | Real-time ticker data retriever | 235 |
| **Level 2 (Order Book)** |
| `retrieve_kraken_live_data_level2.cpp` | Capture aggregated order book data | 320 |
| `process_orderbook_snapshots.cpp` | Generate metrics from raw data | 430 |
| **Level 3 (Order-Level)** |
| `retrieve_kraken_live_data_level3.cpp` | Capture individual order data | 450 |
| `process_level3_snapshots.cpp` | Generate order flow metrics | 480 |

**Total**: ~1915 lines

### 6. Legacy Implementations (Reference Only)

| File | Status | Purpose |
|------|--------|---------|
| `query_live_data_v2.cpp` | Legacy | Original blocking version |
| `query_live_data_v2_refactored.cpp` | Legacy | Refactored blocking version |
| `query_live_data_v2_standalone.cpp` | Legacy | Original demo |
| `query_live_data_v2_standalone_refactored.cpp` | Legacy | Refactored demo |

## Build Outputs

After running `cmake --build build`, you'll have:

### Libraries (static)
```
build/libkraken_common.a                    # Common utilities
build/libcli_utils.a                        # CLI utilities
build/libkraken_websocket_client.a          # WebSocket client (nlohmann)
build/libkraken_websocket_client_simdjson.a # WebSocket client (simdjson)

# Level 2 libraries
build/liborderbook_common.a                 # Order book data structures
build/liborderbook_state.a                  # Order book state rebuilder
build/libsnapshot_csv_writer.a              # CSV writer
build/libjsonl_writer.a                     # JSON Lines writer

# Level 3 libraries
build/liblevel3_common.a                    # Level 3 data structures
build/liblevel3_state.a                     # Level 3 state rebuilder
build/liblevel3_csv_writer.a                # Level 3 CSV writer
build/liblevel3_jsonl_writer.a              # Level 3 JSON Lines writer
build/libkraken_level3_client.a             # Level 3 WebSocket client

# External
build/libsimdjson.a                         # simdjson library
```

### Executables

```
# Educational Examples
build/example_simple_polling          # ⭐ Example 1: Polling
build/example_callback_driven         # ⭐ Example 2: Callbacks
build/example_integration             # ⭐ Example 3: Integration
build/example_integration_cond        # ⭐ Example 3b: Responsive integration
build/example_simdjson_comparison     # ⭐ Example 4: Performance comparison
build/example_template_version        # ⭐ Example 5: Template-based
build/threading_examples              # ⭐ Example 6: Threading patterns
build/sleep_vs_cv                     # ⭐ Example 7: Sleep vs CV

# Production Tools - Level 1
build/retrieve_kraken_live_data_level1  # ⭐⭐ Production: Live ticker retriever

# Production Tools - Level 2
build/retrieve_kraken_live_data_level2  # ⭐⭐ Production: Capture order book data
build/process_orderbook_snapshots       # ⭐⭐ Production: Generate Level 2 metrics

# Production Tools - Level 3
build/retrieve_kraken_live_data_level3  # ⭐⭐ Production: Capture order-level data
build/process_level3_snapshots          # ⭐⭐ Production: Generate Level 3 metrics

# Legacy
build/query_live_data_v2              # Legacy: Blocking version
build/query_live_data_v2_standalone   # Legacy: Demo version
```

## Quick Start Guide

### For New Users

1. **Build everything**:
```bash
cd /export1/rocky/dev/kraken/cpp
cmake -B build -S .
cmake --build build
```

2. **Run simple example**:
```bash
./build/example_simple_polling
```

3. **Read documentation**:
   - Start with: `docs/EXAMPLES_README.md`
   - Then: `docs/QUICK_REFERENCE.md`

### For Level 1 (Ticker Data)

```bash
# Capture real-time ticker data
./build/retrieve_kraken_live_data_level1 -p "BTC/USD,ETH/USD" -o ticker.csv

# Or from a file
echo -e "BTC/USD\nETH/USD\nXRP/USD" > pairs.txt
./build/retrieve_kraken_live_data_level1 -p pairs.txt -o ticker.csv
```

### For Level 2 (Order Book)

**Step 1: Capture raw data**
```bash
# Capture order book data to JSON Lines
./build/retrieve_kraken_live_data_level2 -p "BTC/USD" -d 10 -o raw_book.jsonl
```

**Step 2: Process and analyze**
```bash
# Generate 1-second snapshots with metrics
./build/process_orderbook_snapshots -i raw_book.jsonl --interval 1s -o snapshots.csv

# Generate 5-second snapshots for specific symbol
./build/process_orderbook_snapshots -i raw_book.jsonl --interval 5s --symbol BTC/USD -o btc_5s.csv

# Separate files per symbol
./build/process_orderbook_snapshots -i raw_book.jsonl --interval 1s --separate-files
```

### For Level 3 (Order-Level Data)

**Step 1: Set authentication**
```bash
# Option A: Environment variable (recommended)
export KRAKEN_WS_TOKEN="your_websocket_token_here"

# Option B: Token file
echo "your_websocket_token_here" > ~/.kraken/ws_token
chmod 600 ~/.kraken/ws_token
```

**Step 2: Capture raw order data**
```bash
# Minimal capture
./build/retrieve_kraken_live_data_level3 -p "BTC/USD"

# With live display
./build/retrieve_kraken_live_data_level3 -p "BTC/USD" -v --show-top

# Live order feed
./build/retrieve_kraken_live_data_level3 -p "BTC/USD" --show-orders
```

**Step 3: Process and analyze**
```bash
# Generate 1-second snapshots with order flow metrics
./build/process_level3_snapshots -i kraken_level3.jsonl --interval 1s -o level3_snapshots.csv

# Generate 5-second snapshots for specific symbol
./build/process_level3_snapshots -i kraken_level3.jsonl --interval 5s --symbol BTC/USD -o btc_level3.csv

# Separate files per symbol
./build/process_level3_snapshots -i kraken_level3.jsonl --interval 1s --separate-files
```

### For Integrating into Your Project

1. **Copy library files**:
   - Core: `kraken_common.{hpp,cpp}`, `cli_utils.{hpp,cpp}`
   - Level 1: `kraken_websocket_client_simdjson.{hpp,cpp}`
   - Level 2: `orderbook_*.{hpp,cpp}`, `snapshot_csv_writer.{hpp,cpp}`
   - Level 3: `level3_*.{hpp,cpp}`, `kraken_level3_client.{hpp,cpp}`

2. **Include in your code**:
```cpp
// Level 1 (Ticker)
#include "kraken_websocket_client_simdjson.hpp"

// Level 2 (Order Book)
#include "orderbook_state.hpp"
#include "snapshot_csv_writer.hpp"

// Level 3 (Order-Level)
#include "kraken_level3_client.hpp"
#include "level3_state.hpp"
```

3. **Link libraries**:
```cmake
target_link_libraries(your_app
    kraken_websocket_client_simdjson
    kraken_common
    cli_utils
    orderbook_state  # If using Level 2
    level3_state     # If using Level 3
    simdjson
    ${OPENSSL_LIBRARIES}
    ${Boost_LIBRARIES}
    pthread
)
```

## Design Principles

### 1. Two-Tool Architecture (Level 2 & 3)
- **Separation**: Capture vs Analysis
- **Flexibility**: Process offline, experiment with metrics
- **Reliability**: If processing fails, raw data is preserved

### 2. Adaptive Precision
- **Preserve accuracy**: Full numeric precision in CSV
- **Minimize size**: Remove trailing zeros
- **No data loss**: Critical for financial data

### 3. Dual Indexing (Level 3)
- **Fast lookup**: O(log n) order updates by ID
- **Fast iteration**: O(1) price-level access
- **Trade-off**: 2x memory for better performance

### 4. Authentication (Level 3)
- **Priority order**: --token > --token-file > env var
- **Security**: Avoid hardcoded tokens
- **Flexibility**: Support multiple input methods

### 5. Reusability
- **Library code**: Common utilities, WebSocket clients
- **Example code**: Educational patterns
- **Production tools**: Ready-to-use applications

## Metrics

### Code Organization
```
Core libraries:         ~2,150 lines (reusable)
Level 2 libraries:      ~1,230 lines (order book)
Level 3 libraries:      ~1,850 lines (order-level)
Educational examples:   ~1,180 lines (learning)
Production tools:       ~1,915 lines (ready-to-use)
Legacy code:            ~1,200 lines (reference)
Documentation:          ~6,000 lines (across 20+ files)

Total:                 ~15,500 lines
```

### Build Targets
- 15 static libraries
- 13 executables (8 examples + 5 production tools)
- Auto-downloads 3 header-only libraries
- Links to 2 system libraries (boost, openssl)

## Key Differences: Level 1 vs Level 2 vs Level 3

| Feature | Level 1 (Ticker) | Level 2 (Order Book) | Level 3 (Order-Level) |
|---------|------------------|----------------------|------------------------|
| **Granularity** | OHLC, volume, prices | Aggregated price levels | Individual orders |
| **Data Structure** | TickerRecord | `{price, qty}` | `{order_id, price, qty, timestamp}` |
| **Updates** | Ticker changes | Price level changes | Order events (add/modify/delete) |
| **Authentication** | Not required | Not required | **Required (token)** |
| **Channel** | `ticker` | `book` | `level3` |
| **State Management** | Stateless | Simple map by price | **Dual indexing** (by ID and price) |
| **Metrics** | Basic market data | Market depth, imbalance | + Order flow, queue dynamics |
| **Use Cases** | Price monitoring | Liquidity analysis | Market microstructure research |

## Evolution History

### Phase 1: Initial Implementation (Level 1)
- Created `query_live_data_v2.cpp` (blocking version)
- Direct Python-to-C++ conversion

### Phase 2: Refactoring
- Extracted common code to `kraken_common`
- Created `query_live_data_v2_refactored.cpp`
- Added comprehensive documentation

### Phase 3: Non-blocking Support
- Created monolithic `query_live_data_v2_nonblocking.cpp`
- All code in one file

### Phase 4: Library Architecture
- Extracted `KrakenWebSocketClient` class
- Created reusable library
- Added multiple example applications
- Clean separation of concerns

### Phase 5: Level 2 Implementation
- Designed two-tool architecture (capture + process)
- Implemented order book state rebuilder
- Added CRC32 checksum validation
- Created adaptive precision CSV writer

### Phase 6: Level 3 Implementation ⭐ **Current**
- Implemented authenticated WebSocket client
- Created dual-indexed state rebuilder
- Added order flow metrics
- Comprehensive order-level analytics

## Future Enhancements

### Short-term
- [ ] Unit tests for all libraries
- [ ] Example with error recovery/reconnection
- [ ] Performance benchmarks for Level 3
- [ ] Python bindings (pybind11)

### Long-term
- [ ] Support for trades channel
- [ ] Real-time metric calculation
- [ ] Configuration file support
- [ ] Metrics/monitoring integration
- [ ] Docker container

## Conclusion

The current architecture provides:
- ✅ **Reusable libraries** - Use in any project
- ✅ **Clean separation** - Library vs examples vs tools
- ✅ **Multiple levels** - Ticker, order book, order-level
- ✅ **Two-tool approach** - Capture + process for flexibility
- ✅ **Production ready** - Thread-safe, authenticated, documented
- ✅ **Educational** - Examples for learning patterns
- ✅ **Comprehensive** - Full market data pipeline

**Recommended approach**:
- For simple ticker monitoring: Use Level 1 tools
- For liquidity analysis: Use Level 2 capture + process pipeline
- For market microstructure research: Use Level 3 capture + process pipeline
- For learning: Study the example applications
- For custom projects: Integrate the library files
