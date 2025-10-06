# Kraken Market Data Tools

A comprehensive suite of tools for collecting and analyzing cryptocurrency market data from the Kraken exchange, including Python scripts for quick analysis and high-performance C++ tools for production workloads.

## Overview

This project provides tools for accessing three levels of market data from Kraken's WebSocket v2 API:

- **Level 1 (Ticker)**: OHLC prices, volume, and basic market data
- **Level 2 (Order Book)**: Aggregated order book depth with price levels
- **Level 3 (Order-Level)**: Individual order data with full market microstructure

## Project Structure

```
kraken/
├── README.md                  # This file
├── CLAUDE.md                  # AI assistant instructions
├── .gitignore                 # Git ignore rules
│
├── python/                    # Python scripts
│   ├── rest_api/             # REST API scripts
│   ├── websocket/            # WebSocket scripts
│   ├── schemas/              # JSON schemas
│   └── README.md
│
├── cpp/                       # C++ implementation (production tools)
│   ├── lib/                  # Reusable libraries
│   ├── examples/             # Educational examples & production tools
│   ├── legacy/               # Legacy implementations
│   ├── docs/                 # Comprehensive documentation
│   ├── build/                # Build outputs (generated)
│   └── CMakeLists.txt
│
├── data/                      # Generated data files (gitignored)
│   ├── *.csv                 # CSV outputs
│   ├── *.jsonl               # JSON Lines streaming data
│   └── README.md
│
└── scripts/                   # Utility scripts
    └── test_retrieve_tool.sh
```

## Quick Start

### Python (Quick Analysis)

```bash
# Install dependencies
pip install requests krakenex websockets pandas jsonschema

# List all trading pairs
python python/rest_api/list_all_pairs.py

# Get volume analysis
python python/rest_api/get_volume_all_pairs.py

# Live ticker monitoring (WebSocket v2)
python python/websocket/query_live_data_v2.py

# Order book monitoring
python python/websocket/krakenwsorderbookasync.py BTC/USD 10
```

See [`python/README.md`](python/README.md) for detailed Python documentation.

### C++ (Production Tools)

```bash
# Build everything
cd cpp
cmake -B build -S .
cmake --build build

# Level 1: Ticker data
./build/retrieve_kraken_live_data_level1 -p "BTC/USD,ETH/USD" -o ../data/ticker.csv

# Level 2: Order book (two-step pipeline)
# Step 1: Capture raw data
./build/retrieve_kraken_live_data_level2 -p "BTC/USD" -d 10 -o ../data/raw_book.jsonl

# Step 2: Process to metrics
./build/process_orderbook_snapshots -i ../data/raw_book.jsonl --interval 1s -o ../data/snapshots.csv

# Level 3: Order-level data (requires authentication)
export KRAKEN_WS_TOKEN="your_websocket_token"
./build/retrieve_kraken_live_data_level3 -p "BTC/USD" -d 10 -o ../data/level3_raw.jsonl
./build/process_level3_snapshots -i ../data/level3_raw.jsonl --interval 1s -o ../data/level3_snapshots.csv
```

See [`cpp/docs/PROJECT_STRUCTURE.md`](cpp/docs/PROJECT_STRUCTURE.md) for detailed C++ documentation.

## Features

### Python Scripts
- ✅ REST API integration for static data
- ✅ WebSocket v1 and v2 clients
- ✅ Volume analysis across all pairs
- ✅ Real-time ticker monitoring
- ✅ Order book depth tracking
- ✅ JSON schema validation

### C++ Tools
- ✅ High-performance WebSocket clients
- ✅ Multi-level market data (Level 1, 2, 3)
- ✅ Two-tool architecture (capture + process)
- ✅ Authenticated Level 3 access
- ✅ Dual-indexed order book state
- ✅ Adaptive precision CSV output
- ✅ CRC32 checksum validation
- ✅ Thread-safe, production-ready
- ✅ Comprehensive metrics calculation

## Market Data Levels

### Level 1 (Ticker Data)
**Use case**: Price monitoring, basic market analysis

```bash
# Python
python python/websocket/query_live_data_v2.py

# C++
./cpp/build/retrieve_kraken_live_data_level1 -p "BTC/USD,ETH/USD"
```

**Output**: Timestamp, symbol, bid, ask, last, volume, VWAP, high, low, change

### Level 2 (Order Book)
**Use case**: Liquidity analysis, market depth, spread analysis

```bash
# Capture raw order book data
./cpp/build/retrieve_kraken_live_data_level2 -p "BTC/USD" -d 10

# Generate periodic snapshots with metrics
./cpp/build/process_orderbook_snapshots -i data/raw.jsonl --interval 1s
```

**Metrics**: Best bid/ask, spread, volume imbalance, depth at various BPS, mid-price

### Level 3 (Order-Level)
**Use case**: Market microstructure research, order flow analysis, queue dynamics

```bash
# Requires authentication token
export KRAKEN_WS_TOKEN="your_token"

# Capture individual orders
./cpp/build/retrieve_kraken_live_data_level3 -p "BTC/USD" -d 10 --show-top

# Generate order flow metrics
./cpp/build/process_level3_snapshots -i data/level3.jsonl --interval 1s
```

**Additional Metrics**: Order counts, average order sizes, add/modify/delete events, order arrival rate, cancel rate

## Authentication (Level 3 Only)

Level 3 data requires a WebSocket authentication token from Kraken.

**Setup options:**

```bash
# Option 1: Environment variable (recommended)
export KRAKEN_WS_TOKEN="your_websocket_token"

# Option 2: Token file
echo "your_websocket_token" > ~/.kraken/ws_token
chmod 600 ~/.kraken/ws_token
./cpp/build/retrieve_kraken_live_data_level3 -p "BTC/USD" --token-file ~/.kraken/ws_token

# Option 3: Command line (testing only)
./cpp/build/retrieve_kraken_live_data_level3 -p "BTC/USD" --token "your_token"
```

**Priority**: `--token` > `--token-file` > `KRAKEN_WS_TOKEN` env var

## Data Pipeline Example

### End-to-End Level 2 Workflow

```bash
# 1. Capture raw order book data (run for desired duration)
./cpp/build/retrieve_kraken_live_data_level2 -p "BTC/USD,ETH/USD" -d 10 -o data/raw_book.jsonl
# Let it run... Press Ctrl+C to stop

# 2. Process to 1-second snapshots
./cpp/build/process_orderbook_snapshots \
  -i data/raw_book.jsonl \
  --interval 1s \
  -o data/snapshots_1s.csv

# 3. Analyze in Python
python
>>> import pandas as pd
>>> df = pd.read_csv('data/snapshots_1s.csv')
>>> df['timestamp'] = pd.to_datetime(df['timestamp'])
>>> print(df[['timestamp', 'symbol', 'mid_price', 'spread_bps', 'imbalance']].head(10))
```

### End-to-End Level 3 Workflow

```bash
# 1. Set authentication
export KRAKEN_WS_TOKEN="your_token"

# 2. Capture order-level data
./cpp/build/retrieve_kraken_live_data_level3 -p "BTC/USD" -d 10 -v --show-top -o data/level3.jsonl
# Monitor live order flow... Press Ctrl+C to stop

# 3. Generate order flow metrics
./cpp/build/process_level3_snapshots \
  -i data/level3.jsonl \
  --interval 1s \
  -o data/level3_metrics.csv

# 4. Analyze order flow
python
>>> import pandas as pd
>>> df = pd.read_csv('data/level3_metrics.csv')
>>> print(df[['timestamp', 'order_arrival_rate', 'order_cancel_rate', 'bid_order_count', 'ask_order_count']].head())
```

## Performance

### Python vs C++

| Metric | Python | C++ |
|--------|--------|-----|
| **Execution** | Interpreted | Compiled (native) |
| **Latency** | ~10-50ms | ~1-5ms |
| **Memory** | Higher | Lower |
| **CPU** | Higher | Lower |
| **Best for** | Quick analysis, prototyping | Production, high-frequency |

### C++ Performance Features
- **simdjson**: 2-5x faster JSON parsing
- **Thread-safe**: Non-blocking WebSocket client
- **Dual indexing**: O(log n) order lookup, O(1) price iteration
- **Adaptive precision**: Minimal output size, no data loss

## Documentation

### Getting Started
- 📖 [`python/README.md`](python/README.md) - Python scripts guide
- 📖 [`cpp/docs/EXAMPLES_README.md`](cpp/docs/EXAMPLES_README.md) - C++ examples ⭐ **START HERE**
- 📖 [`cpp/docs/PROJECT_STRUCTURE.md`](cpp/docs/PROJECT_STRUCTURE.md) - C++ architecture overview

### Order Book Documentation
- 📖 [`cpp/docs/LEVEL2_ORDERBOOK_DESIGN.md`](cpp/docs/LEVEL2_ORDERBOOK_DESIGN.md) - Level 2 design decisions
- 📖 [`cpp/docs/LEVEL3_ORDERBOOK_DESIGN.md`](cpp/docs/LEVEL3_ORDERBOOK_DESIGN.md) - Level 3 design decisions
- 📖 [`cpp/docs/LEVEL3_IMPLEMENTATION_SUMMARY.md`](cpp/docs/LEVEL3_IMPLEMENTATION_SUMMARY.md) - Level 3 status

### API References
- 📖 [`cpp/docs/QUICK_REFERENCE.md`](cpp/docs/QUICK_REFERENCE.md) - C++ API quick reference
- 📖 [`cpp/docs/CLI_UTILS_QUICK_REFERENCE.md`](cpp/docs/CLI_UTILS_QUICK_REFERENCE.md) - CLI utilities

### Technical Details
- 📖 [`cpp/docs/BLOCKING_VS_NONBLOCKING.md`](cpp/docs/BLOCKING_VS_NONBLOCKING.md) - Threading models
- 📖 [`cpp/docs/JSON_LIBRARY_COMPARISON.md`](cpp/docs/JSON_LIBRARY_COMPARISON.md) - Performance analysis
- 📖 [`cpp/docs/DEVELOPMENT_NOTES.md`](cpp/docs/DEVELOPMENT_NOTES.md) - Build troubleshooting

## Dependencies

### Python
```bash
pip install requests krakenex websockets pandas jsonschema
```

### C++ (auto-downloaded by CMake)
- **websocketpp** - WebSocket library (header-only)
- **simdjson** - Fast JSON parser
- **nlohmann/json** - JSON library (header-only)

### C++ System Requirements
```bash
# RHEL/CentOS/Rocky Linux
sudo yum install cmake gcc-c++ openssl-devel boost-devel

# Ubuntu/Debian
sudo apt install cmake g++ libssl-dev libboost-dev
```

## Use Cases

### Academic Research
- Market microstructure analysis
- Order flow dynamics
- Liquidity provision patterns
- High-frequency trading behavior

### Trading
- Real-time market monitoring
- Liquidity analysis
- Spread tracking
- Order book imbalance signals

### Data Science
- Price prediction models
- Volume analysis
- Market regime detection
- Feature engineering for ML

## Contributing

This project follows a clean architecture:
1. **Libraries** in `cpp/lib/` - Reusable components
2. **Examples** in `cpp/examples/` - Educational code
3. **Production tools** in `cpp/examples/` - Ready-to-use applications
4. **Documentation** in `cpp/docs/` - Comprehensive guides

## License

See individual files for licensing information.

## External Resources

- [Kraken API Documentation](https://docs.kraken.com/api/)
- [Kraken WebSocket v2](https://docs.kraken.com/api/docs/websocket-v2/)
- [Kraken REST API](https://docs.kraken.com/api/docs/rest-api/)
- [simdjson](https://github.com/simdjson/simdjson)
- [websocketpp](https://github.com/zaphoyd/websocketpp)

## Support

For issues with:
- **Python scripts**: See [`python/README.md`](python/README.md)
- **C++ tools**: See [`cpp/docs/DEVELOPMENT_NOTES.md`](cpp/docs/DEVELOPMENT_NOTES.md)
- **Build issues**: See [`cpp/docs/DEVELOPMENT_NOTES.md`](cpp/docs/DEVELOPMENT_NOTES.md)

---

**Project Status**: Active development - Level 3 implementation complete ✅

**Last Updated**: October 2025
