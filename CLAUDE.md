# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Project Overview

This repository contains Python scripts for interacting with the Kraken cryptocurrency exchange API, both REST and WebSocket endpoints. The scripts primarily focus on market data analysis, volume calculations, and real-time order book/ticker monitoring.

## Key Scripts and Their Purpose

### Data Collection Scripts

- **simple_data.py** - Basic REST API example fetching BTC/USD ticker data using `requests`
- **simple_query.py** - Private API query example using `krakenex` library (requires API key at `/export1/rocky/.kraken/kraken.key`)
- **list_all_pairs.py** - Lists all available trading pairs from Kraken's AssetPairs endpoint
- **query_live_data.py** - WebSocket v1 client that subscribes to ticker updates for multiple pairs and saves historical data to CSV
- **query_live_data_v2.py** - WebSocket v2 client (updated API) with cleaner message format and additional fields (change, change_pct)
- **cpp/query_live_data_v2.cpp** - C++ implementation of WebSocket v2 client using websocketpp and nlohmann/json libraries
- **krakenwsorderbookasync.py** - WebSocket order book monitor with depth tracking (usage: `python krakenwsorderbookasync.py <symbol> <depth>`)

### Volume Analysis Scripts

- **get_volume_all_pairs.py** - Comprehensive volume analysis script that:
  - Fetches all Kraken trading pairs
  - Retrieves ticker data in chunks (30 pairs at a time with 0.2s pause between requests)
  - Builds USD conversion map automatically from all XXX/USD pairs
  - Calculates 24h volume in USD notional terms for all pairs
  - Outputs top 500 pairs by USD volume to `kraken_top500_usd_volume.csv`

- **find_missing_conversions.py** - Analysis script to identify quote currencies missing from the USD conversion map by comparing `quote_volume_24h` and `usd_volume_24h` columns

## Running Scripts

### Basic Data Fetching
```bash
python simple_data.py
python list_all_pairs.py
```

### Live Data Collection (WebSocket)
```bash
# Ticker data v1 (modify pairs in script)
python query_live_data.py

# Ticker data v2 Python (modify pairs in script)
python query_live_data_v2.py

# Ticker data v2 C++ (build first, see cpp/README.md)
cd cpp && mkdir build && cd build && cmake .. && cmake --build .
./query_live_data_v2

# Order book (real-time, specify symbol and depth)
python krakenwsorderbookasync.py XBT/USD 10
```

### Volume Analysis Pipeline
```bash
# Generate volume rankings
python get_volume_all_pairs.py

# Analyze for missing conversions
python find_missing_conversions.py
```

### Private API Access
```bash
# Requires API key file at /export1/rocky/.kraken/kraken.key
python simple_query.py
```

## Architecture Notes

### API Interaction Patterns

**REST API**: Uses `requests` library with chunking for bulk ticker queries (30 pairs/request, 0.2s delay)
- Public endpoint: `https://api.kraken.com/0/public/`
- Asset pairs endpoint for metadata
- Ticker endpoint for price/volume data

**WebSocket**: Multiple implementations and API versions
- **v1 API** (`wss://ws.kraken.com`):
  - `query_live_data.py` - Uses `websockets` library (async/await)
  - `krakenwsorderbookasync.py` - Uses `websocket` library (thread-based)
  - Subscription format: `{"event": "subscribe", "pair": [...], "subscription": {"name": "ticker"}}`

- **v2 API** (`wss://ws.kraken.com/v2`):
  - `query_live_data_v2.py` - Uses `websockets` library (async/await)
  - Subscription format: `{"method": "subscribe", "params": {"channel": "ticker", "symbol": [...]}}`
  - Cleaner response format with `channel`, `type`, and `data` array
  - Additional fields: `change` (absolute price change), `change_pct` (percentage change)

### Currency Conversion Logic (get_volume_all_pairs.py)

The `build_usd_conversion()` function auto-generates conversion factors:
1. Hardcoded USD-pegged stablecoins (USDT, USDC, DAI, etc.) = 1.0
2. Scans ticker results for XXX/USD pairs → adds direct conversions
3. Scans for USD/XXX pairs → adds inverted conversions (1/price)
4. Missing conversions default to 1.0 (can be identified via `find_missing_conversions.py`)

### Data Format

**CSV Outputs**:
- `kraken_ticker_history.csv` - Historical ticker updates from v1 API (timestamp, pair, bid/ask prices & volumes, VWAP, trade counts)
- `kraken_ticker_history_v2.csv` - Historical ticker updates from v2 API (timestamp, pair, bid/ask, last, volume, vwap, high, low, change, change_pct)
- `kraken_top500_usd_volume.csv` - Columns: `pair`, `base_volume_24h`, `quote_volume_24h`, `usd_volume_24h`
- `kraken_pairs_liquidity.csv` - Liquidity data (script not provided)

**API Name Mapping**: Scripts maintain `api_to_ws` dict mapping Kraken's API pair names to WebSocket names (wsname)

### API Schemas

**schemas/** directory contains JSON Schema definitions for Kraken API data structures:
- `websocket_v2_ticker_schema.json` - Complete JSON Schema (Draft 7) for WebSocket v2 ticker channel
  - Defines: `SubscriptionRequest`, `SubscriptionResponse`, `UnsubscribeRequest`, `UnsubscribeResponse`, `TickerData`, `TickerUpdate`, `Heartbeat`
  - Source: https://docs.kraken.com/api/docs/websocket-v2/ticker/
  - Use with `jsonschema` library for validation in Python

Example validation:
```python
import json, jsonschema
with open('schemas/websocket_v2_ticker_schema.json') as f:
    schema = json.load(f)
jsonschema.validate(message, schema['definitions']['TickerUpdate'])
```

## Dependencies

### Python
- `requests` - REST API calls
- `krakenex` - Kraken API wrapper for private endpoints
- `websockets` / `websocket` - WebSocket clients
- `pandas` - Data manipulation and CSV output
- `asyncio` - Async WebSocket handling
- `jsonschema` - Schema validation (optional, for validated version)

### C++ (in cpp/ directory)
- `websocketpp` - WebSocket client library (header-only)
- `nlohmann/json` - JSON parsing (header-only)
- `Boost` - ASIO for networking
- `OpenSSL` - TLS/SSL support
- `CMake` - Build system (version 3.10+)

See `cpp/README.md` for detailed C++ setup instructions.
