# Python Scripts for Kraken API

This directory contains Python scripts for interacting with the Kraken cryptocurrency exchange API, both REST and WebSocket endpoints.

## Directory Structure

```
python/
├── rest_api/          # REST API scripts
│   ├── simple_data.py              # Basic ticker data fetch
│   ├── simple_query.py             # Private API query example
│   ├── list_all_pairs.py           # List all trading pairs
│   ├── get_volume_all_pairs.py     # Volume analysis for all pairs
│   └── find_missing_conversions.py # Analyze missing USD conversions
│
├── websocket/         # WebSocket API scripts
│   ├── query_live_data.py          # WebSocket v1 ticker data
│   ├── query_live_data_v2.py       # WebSocket v2 ticker data
│   ├── query_live_data_v2_validated.py  # WebSocket v2 with schema validation
│   └── krakenwsorderbookasync.py   # WebSocket order book monitor
│
└── schemas/           # JSON Schema definitions
    ├── README.md
    └── websocket_v2_ticker_schema.json
```

## Requirements

### Python Dependencies

```bash
pip install requests krakenex websockets pandas jsonschema
```

### API Key Setup (for private endpoints)

Create a Kraken API key file:
```bash
mkdir -p ~/.kraken
# Add your API key and secret to this file
# Format: key on line 1, secret on line 2
```

## REST API Scripts

### Basic Data Fetching

**simple_data.py** - Fetch BTC/USD ticker data
```bash
python python/rest_api/simple_data.py
```

**list_all_pairs.py** - List all available trading pairs
```bash
python python/rest_api/list_all_pairs.py
```

### Volume Analysis

**get_volume_all_pairs.py** - Analyze 24h volume for all pairs in USD
```bash
python python/rest_api/get_volume_all_pairs.py
# Output: kraken_top500_usd_volume.csv
```

Features:
- Fetches all trading pairs
- Retrieves ticker data in chunks (30 pairs at a time)
- Builds USD conversion map automatically
- Calculates 24h volume in USD notional terms
- Outputs top 500 pairs by volume

**find_missing_conversions.py** - Identify missing USD conversions
```bash
python python/rest_api/find_missing_conversions.py
# Analyzes kraken_top500_usd_volume.csv
```

### Private API

**simple_query.py** - Query private account data
```bash
python python/rest_api/simple_query.py
# Requires API key at ~/.kraken/kraken.key
```

## WebSocket Scripts

### WebSocket v1 (Legacy)

**query_live_data.py** - Subscribe to ticker updates (v1 API)
```bash
python python/websocket/query_live_data.py
# Modify pairs in script before running
# Output: kraken_ticker_history.csv
```

### WebSocket v2

**query_live_data_v2.py** - Subscribe to ticker updates (v2 API)
```bash
python python/websocket/query_live_data_v2.py
# Modify pairs in script before running
# Output: kraken_ticker_history_v2.csv
```

Features:
- Cleaner v2 API message format
- Additional fields: change, change_pct
- Better error handling

**query_live_data_v2_validated.py** - v2 with JSON schema validation
```bash
python python/websocket/query_live_data_v2_validated.py
# Uses schemas/websocket_v2_ticker_schema.json for validation
```

### Order Book Monitor

**krakenwsorderbookasync.py** - Real-time order book monitoring
```bash
python python/websocket/krakenwsorderbookasync.py <symbol> <depth>

# Examples:
python python/websocket/krakenwsorderbookasync.py XBT/USD 10
python python/websocket/krakenwsorderbookasync.py ETH/USD 25
```

Features:
- Real-time order book depth tracking
- Displays top N levels on each side
- Updates on every WebSocket message

## Data Flow Examples

### Volume Analysis Pipeline

```bash
# Step 1: Fetch volume data for all pairs
python python/rest_api/get_volume_all_pairs.py

# Step 2: Analyze for missing conversions
python python/rest_api/find_missing_conversions.py

# Step 3: Analyze in Python/R
python
>>> import pandas as pd
>>> df = pd.read_csv('../data/kraken_top500_usd_volume.csv')
>>> print(df.head(10))
```

### Live Ticker Monitoring

```bash
# Collect data
python python/websocket/query_live_data_v2.py
# Let it run for desired duration (Ctrl+C to stop)

# Analyze
python
>>> import pandas as pd
>>> df = pd.read_csv('../data/kraken_ticker_history_v2.csv')
>>> df['timestamp'] = pd.to_datetime(df['timestamp'])
>>> print(df.describe())
```

## API Endpoints Used

### REST API
- `https://api.kraken.com/0/public/AssetPairs` - Asset pairs metadata
- `https://api.kraken.com/0/public/Ticker` - Ticker data (price/volume)
- `https://api.kraken.com/0/private/*` - Private endpoints (requires auth)

### WebSocket
- **v1**: `wss://ws.kraken.com`
  - Channel: `ticker`
  - Subscription: `{"event": "subscribe", "pair": [...], "subscription": {"name": "ticker"}}`

- **v2**: `wss://ws.kraken.com/v2`
  - Channel: `ticker`
  - Subscription: `{"method": "subscribe", "params": {"channel": "ticker", "symbol": [...]}}`

## Output Files

Scripts generate data files in the `../data/` directory:
- `kraken_ticker_history.csv` - Historical ticker data (v1)
- `kraken_ticker_history_v2.csv` - Historical ticker data (v2)
- `kraken_top500_usd_volume.csv` - Volume rankings
- `kraken_pairs_liquidity.csv` - Liquidity data
- `kraken_usd_volume.csv` - USD volume data
- `missing_conversions.json` - Missing conversion pairs

## For C++ Implementation

For high-performance production usage, see the C++ implementation in `../cpp/`:
- Faster execution (compiled)
- Lower latency
- Better resource management
- Production-ready tools for Level 1, 2, and 3 data

## References

- [Kraken API Documentation](https://docs.kraken.com/api/)
- [Kraken WebSocket v2 Documentation](https://docs.kraken.com/api/docs/websocket-v2/)
- [Kraken REST API Reference](https://docs.kraken.com/api/docs/rest-api/get-ticker-information/)
