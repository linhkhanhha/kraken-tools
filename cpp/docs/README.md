# C++ WebSocket Client for Kraken API v2

This is a C++ implementation of the Kraken WebSocket v2 ticker client, equivalent to `query_live_data_v2.py`.

## Dependencies

### Required Libraries

1. **websocketpp** - WebSocket client library (header-only)
   - GitHub: https://github.com/zaphoyd/websocketpp
   ```bash
   git clone https://github.com/zaphoyd/websocketpp.git
   ```

2. **nlohmann/json** - JSON library for C++ (header-only)
   - GitHub: https://github.com/nlohmann/json
   ```bash
   # Single header file
   wget https://github.com/nlohmann/json/releases/download/v3.11.3/json.hpp
   # Or use package manager
   sudo apt-get install nlohmann-json3-dev  # Ubuntu/Debian
   ```

3. **Boost** - For ASIO networking
   ```bash
   sudo apt-get install libboost-all-dev  # Ubuntu/Debian
   sudo yum install boost-devel           # RHEL/CentOS
   ```

4. **OpenSSL** - For TLS/SSL support
   ```bash
   sudo apt-get install libssl-dev        # Ubuntu/Debian
   sudo yum install openssl-devel         # RHEL/CentOS
   ```

## Build Instructions

### Quick Start (Using CMake - Recommended)

CMake will automatically:
1. Try to install boost-devel if missing (requires sudo)
2. Download websocketpp (header-only library)
3. Download nlohmann/json (header-only library)

```bash
# Configure and build (one command does it all!)
cmake -B build -S .

# If boost-devel installation fails, install manually:
sudo yum install boost-devel

# Then retry
cmake -B build -S .

# Build
cmake --build build

# Run
./build/query_live_data_v2
```

### Alternative: Using Makefile

```bash
# Install all dependencies automatically
make install-deps

# Build the project
make

# Run
./query_live_data_v2
```

### Manual Build with g++

```bash
g++ -std=c++17 query_live_data_v2.cpp \
    -I/path/to/websocketpp \
    -I/path/to/json/include \
    -lssl -lcrypto -lboost_system -lpthread \
    -o query_live_data_v2

./query_live_data_v2
```

## Usage

1. **Start the client**:
   ```bash
   ./query_live_data_v2
   ```

2. **Stop with Ctrl+C**:
   - The program will automatically save data to `kraken_ticker_history_v2.csv`

3. **Modify symbols** (in `query_live_data_v2.cpp`):
   ```cpp
   {"symbol", {"BTC/USD", "ETH/USD", "SOL/USD"}},  // Change symbols here
   ```

## Features

- **WebSocket v2 API** - Connects to `wss://ws.kraken.com/v2`
- **Real-time ticker updates** - Receives BTC/USD, ETH/USD, SOL/USD by default
- **CSV export** - Saves all ticker history on exit (Ctrl+C)
- **TLS/SSL support** - Secure WebSocket connection
- **Error handling** - JSON parsing and connection error handling
- **Signal handling** - Graceful shutdown with Ctrl+C

## Output Format

CSV file contains:
- timestamp (UTC)
- pair (symbol)
- type (snapshot/update)
- bid, bid_qty
- ask, ask_qty
- last (last trade price)
- volume (24h)
- vwap (volume-weighted average price)
- low, high (24h)
- change, change_pct (24h)

## Project Structure

```
query_live_data_v2.cpp      # Main C++ source file
CMakeLists.txt              # CMake build configuration
README_CPP.md               # This file
kraken_ticker_history_v2.csv # Output CSV (generated at runtime)
```

## Comparison with Python Version

| Feature | Python | C++ |
|---------|--------|-----|
| WebSocket Library | websockets | websocketpp |
| JSON Library | json (built-in) | nlohmann/json |
| Async Model | asyncio | Boost.Asio |
| Performance | Good | Better (compiled) |
| Dependencies | Minimal | More setup required |

## Troubleshooting

### Missing websocketpp
```bash
# Clone and set include path
git clone https://github.com/zaphoyd/websocketpp.git
export CPLUS_INCLUDE_PATH=/path/to/websocketpp:$CPLUS_INCLUDE_PATH
```

### Missing nlohmann/json
```bash
# Download single header
mkdir -p include/nlohmann
cd include/nlohmann
wget https://github.com/nlohmann/json/releases/download/v3.11.3/json.hpp
cd ../..
export CPLUS_INCLUDE_PATH=$(pwd)/include:$CPLUS_INCLUDE_PATH
```

### Boost not found
```bash
sudo apt-get install libboost-all-dev
# Or specify path
cmake -DBOOST_ROOT=/path/to/boost ..
```

## Performance Notes

- C++ version has lower latency and memory footprint
- Suitable for high-frequency data collection
- Can be extended for algorithmic trading applications
- Consider using lock-free data structures for better performance if needed
