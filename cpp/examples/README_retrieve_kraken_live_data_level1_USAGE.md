# Kraken Live Data Retriever - Usage Examples

## Quick Start

### 1. Build the Tool

```bash
cd /export1/rocky/dev/kraken/cpp
mkdir -p build && cd build
cmake ..
cmake --build . --target retrieve_kraken_live_data_level1
```

### 2. Run with Direct Pairs

```bash
cd /export1/rocky/dev/kraken/cpp/build
./retrieve_kraken_live_data_level1 -p "BTC/USD,ETH/USD,SOL/USD"
```

### 3. Run with CSV File

```bash
# Monitor top 10 pairs by volume
./retrieve_kraken_live_data_level1 -p /export1/rocky/dev/kraken/kraken_usd_volume.csv:pair:10

# Monitor all pairs in CSV
./retrieve_kraken_live_data_level1 -p /export1/rocky/dev/kraken/kraken_usd_volume.csv:pair
```

## Detailed Examples

### Example 1: Monitor Major Cryptocurrencies

Monitor BTC, ETH, SOL, XRP, and ADA against USD:

```bash
./retrieve_kraken_live_data_level1 -p "BTC/USD,ETH/USD,SOL/USD,XRP/USD,ADA/USD"
```

**Expected Output:**
```
==================================================
Kraken Live Data Retriever - Level 1
==================================================
Subscribing to 5 pairs:
  - BTC/USD
  - ETH/USD
  - SOL/USD
  - XRP/USD
  - ADA/USD

[STATUS] WebSocket connected
[UPDATE] BTC/USD | Last: $123900 | Bid: $123900 | Ask: $123900 | Vol: 1856.89
[UPDATE] ETH/USD | Last: $4576.3 | Bid: $4576.3 | Ask: $4576.92 | Vol: 35054
...
```

### Example 2: Monitor Top 10 Pairs by Volume

First generate the volume rankings, then monitor:

```bash
# Step 1: Generate volume rankings
cd /export1/rocky/dev/kraken
python get_volume_all_pairs.py

# Step 2: Monitor top 10
./cpp/build/retrieve_kraken_live_data_level1 -p kraken_usd_volume.csv:pair:10
```

**Expected Output:**
```
==================================================
Kraken Live Data Retriever - Level 1
==================================================
Subscribing to 10 pairs:
  - USDT/USD
  - ETH/USD
  - USDC/EUR
  - XBT/USD
  - USDC/USD
  - USDT/EUR
  - SOL/USD
  - XRP/USD
  - USDC/USDT
  - XBT/EUR

[UPDATE] USDT/USD | Last: $1.00052 | Bid: $1.00051 | Ask: $1.00052 | Vol: 3.54389e+08
...
```

### Example 3: Monitor All USD Pairs

Create a custom CSV file with USD pairs:

```bash
# Create USD pairs file
cat > usd_pairs.csv << 'EOF'
pair
BTC/USD
ETH/USD
SOL/USD
XRP/USD
ADA/USD
MATIC/USD
AVAX/USD
DOT/USD
LINK/USD
UNI/USD
EOF

# Monitor them
./retrieve_kraken_live_data_level1 -p usd_pairs.csv:pair
```

### Example 4: Monitor Specific Stablecoins

Monitor stablecoin pairs:

```bash
./retrieve_kraken_live_data_level1 -p "USDT/USD,USDC/USD,DAI/USD,USDT/EUR,USDC/EUR"
```

### Example 5: Long-Running Data Collection

Run for an extended period with output redirection:

```bash
# Run for 1 hour and save console output
timeout 3600 ./retrieve_kraken_live_data_level1 -p "BTC/USD,ETH/USD,SOL/USD" \
  > live_data_log.txt 2>&1

# Check the saved CSV
head kraken_ticker_live_level1.csv
wc -l kraken_ticker_live_level1.csv
```

### Example 6: Error Handling Examples

#### Missing -p argument:
```bash
$ ./retrieve_kraken_live_data_level1
Error: -p argument is required
Usage: ./retrieve_kraken_live_data_level1 -p <pairs_specification>
...
```

#### Invalid CSV column:
```bash
$ ./retrieve_kraken_live_data_level1 -p kraken_usd_volume.csv:invalid_column:5
Error: Column 'invalid_column' not found in CSV
Available columns: pair, base_volume_24h, quote_volume_24h, usd_volume_24h
Error: No valid pairs found
```

#### File not found:
```bash
$ ./retrieve_kraken_live_data_level1 -p nonexistent.csv:pair:5
Error: Cannot open file: nonexistent.csv
Error: No valid pairs found
```

## Integration with Python Scripts

### Pipeline 1: Volume Analysis + Live Monitoring

```bash
#!/bin/bash
# volume_monitor_pipeline.sh

cd /export1/rocky/dev/kraken

# Step 1: Get latest volume rankings
echo "Fetching volume data..."
python get_volume_all_pairs.py

# Step 2: Monitor top 20 pairs
echo "Starting live monitoring of top 20 pairs..."
./cpp/build/retrieve_kraken_live_data_level1 -p kraken_top500_usd_volume.csv:pair:20
```

### Pipeline 2: Custom Pair Selection

```bash
#!/bin/bash
# custom_pair_monitor.sh

cd /export1/rocky/dev/kraken

# Extract USD pairs with volume > $10M
awk -F',' 'NR==1 || ($1 ~ /\/USD$/ && $4 > 10000000) {print $1}' \
  kraken_usd_volume.csv > high_volume_usd_pairs.csv

# Monitor them
./cpp/build/retrieve_kraken_live_data_level1 -p high_volume_usd_pairs.csv:pair
```

### Pipeline 3: Python Analysis of Live Data

```python
#!/usr/bin/env python3
# analyze_live_data.py

import pandas as pd
import sys

if len(sys.argv) != 2:
    print("Usage: python analyze_live_data.py kraken_ticker_live_level1.csv")
    sys.exit(1)

# Read live data
df = pd.read_csv(sys.argv[1])

# Calculate statistics per pair
stats = df.groupby('pair').agg({
    'last': ['mean', 'std', 'min', 'max'],
    'volume': ['sum', 'mean'],
    'change_pct': ['mean', 'std']
}).round(2)

print("\nLive Data Statistics:")
print("=" * 80)
print(stats)

# Find most volatile pairs
volatility = df.groupby('pair')['change_pct'].std().sort_values(ascending=False)
print("\nMost Volatile Pairs:")
print(volatility.head(10))
```

## Production Deployment Examples

### Example 7: Systemd Service

Create a systemd service for continuous monitoring:

```bash
# /etc/systemd/system/kraken-monitor.service
[Unit]
Description=Kraken Live Data Monitor
After=network.target

[Service]
Type=simple
User=rocky
WorkingDirectory=/export1/rocky/dev/kraken/cpp/build
ExecStart=/export1/rocky/dev/kraken/cpp/build/retrieve_kraken_live_data_level1 \
  -p /export1/rocky/dev/kraken/kraken_usd_volume.csv:pair:20
Restart=always
RestartSec=10

[Install]
WantedBy=multi-user.target
```

Enable and start:
```bash
sudo systemctl enable kraken-monitor
sudo systemctl start kraken-monitor
sudo systemctl status kraken-monitor
```

### Example 8: Cron Job with Rotation

Run for 1 hour every hour with log rotation:

```bash
# Add to crontab: crontab -e
0 * * * * cd /export1/rocky/dev/kraken && ./scripts/hourly_monitor.sh
```

```bash
#!/bin/bash
# scripts/hourly_monitor.sh

cd /export1/rocky/dev/kraken/cpp/build

# Create timestamp
TIMESTAMP=$(date +%Y%m%d_%H%M%S)

# Run for 55 minutes (leave 5 minutes for processing)
timeout 3300 ./retrieve_kraken_live_data_level1 \
  -p /export1/rocky/dev/kraken/kraken_usd_volume.csv:pair:20 \
  > /export1/rocky/logs/kraken_monitor_${TIMESTAMP}.log 2>&1

# Move CSV to archive
if [ -f kraken_ticker_live_level1.csv ]; then
  mv kraken_ticker_live_level1.csv \
    /export1/rocky/data/kraken_ticker_${TIMESTAMP}.csv
fi
```

## Performance Tips

### Optimal Pair Counts

| Pair Count | Update Frequency | Console Output | Recommended Use Case |
|------------|------------------|----------------|----------------------|
| 1-5        | High             | Clean          | Focused monitoring   |
| 5-20       | Medium           | Manageable     | General monitoring   |
| 20-50      | Medium           | Busy           | Broad monitoring     |
| 50-100     | Lower            | Overwhelming   | Data collection      |
| 100+       | Lower            | Unusable       | CSV-only mode needed |

### Reducing Console Noise

For large pair counts, redirect output:

```bash
# Quiet mode (only errors to console)
./retrieve_kraken_live_data_level1 -p file.csv:pair:100 > /dev/null

# Save to log file
./retrieve_kraken_live_data_level1 -p file.csv:pair:100 > monitor.log 2>&1

# Run in background
nohup ./retrieve_kraken_live_data_level1 -p file.csv:pair:100 &
```

## Advanced CSV Filtering

### Filter by Trading Volume

```bash
# Monitor pairs with volume > $50M
awk -F',' 'NR==1 || $4 > 50000000 {print $1}' kraken_usd_volume.csv > temp.csv
./retrieve_kraken_live_data_level1 -p temp.csv:pair
```

### Filter by Quote Currency

```bash
# Monitor only USD pairs
awk -F',' 'NR==1 || $1 ~ /\/USD$/ {print $1}' kraken_usd_volume.csv > usd_only.csv
./retrieve_kraken_live_data_level1 -p usd_only.csv:pair

# Monitor only EUR pairs
awk -F',' 'NR==1 || $1 ~ /\/EUR$/ {print $1}' kraken_usd_volume.csv > eur_only.csv
./retrieve_kraken_live_data_level1 -p eur_only.csv:pair
```

### Filter by Base Currency

```bash
# Monitor all BTC pairs
awk -F',' 'NR==1 || $1 ~ /^BTC\// {print $1}' kraken_usd_volume.csv > btc_pairs.csv
./retrieve_kraken_live_data_level1 -p btc_pairs.csv:pair
```

## Troubleshooting

### Connection Issues

```bash
# Test Kraken API connectivity
curl -s https://api.kraken.com/0/public/SystemStatus | jq

# Check if WebSocket port is accessible
nc -zv ws.kraken.com 443
```

### CSV Parsing Issues

```bash
# Verify CSV format
head -3 kraken_usd_volume.csv

# Check for proper headers
head -1 kraken_usd_volume.csv | tr ',' '\n'

# Test with verbose parsing
./retrieve_kraken_live_data_level1 -p kraken_usd_volume.csv:invalid_col:5 2>&1 | grep "Available columns"
```

## Next Steps

After successfully running the tool, you can:

1. **Analyze the CSV data** with pandas or other tools
2. **Build trading strategies** based on real-time price movements
3. **Create alerts** for significant price changes
4. **Generate reports** on market activity
5. **Extend the tool** to support Level 2 order book data

See `cpp/examples/README_retrieve_kraken_live_data_level1.md` for full documentation.
