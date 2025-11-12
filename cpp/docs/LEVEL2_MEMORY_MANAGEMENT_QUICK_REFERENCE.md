# Level 2 Order Book - Memory Management Quick Reference

**Last Updated**: 2025-11-12

---

## CLI Options Summary

```bash
./retrieve_kraken_live_data_level2 [options]
```

### Periodic Flushing Options

| Flag | Type | Default | Description |
|------|------|---------|-------------|
| `-f, --flush-interval` | seconds | 30 | Flush interval (0 = disabled) |
| `-m, --memory-threshold` | bytes | 10485760 | Memory threshold (0 = disabled) |

### Segmentation Options

| Flag | Type | Default | Description |
|------|------|---------|-------------|
| `--hourly` | flag | off | Enable hourly file segmentation |
| `--daily` | flag | off | Enable daily file segmentation |

**Note**: `--hourly` and `--daily` are mutually exclusive.

---

## Common Usage Patterns

### Pattern 1: Default (Recommended)

```bash
./retrieve_kraken_live_data_level2 -p "BTC/USD" -d 10
```

- Flush every 30s OR 10MB
- Single output file
- **Use when**: Short to medium runs (hours to days)

---

### Pattern 2: Hourly Segmentation (Long-Running)

```bash
./retrieve_kraken_live_data_level2 -p "BTC/USD" -d 10 --hourly
```

- One file per hour: `output.20251112_10.jsonl`, `output.20251112_11.jsonl`
- Flush every 30s OR 10MB
- **Use when**: Multi-day collection, manageable file sizes

---

### Pattern 3: Daily Segmentation

```bash
./retrieve_kraken_live_data_level2 -p "BTC/USD" -d 10 --daily
```

- One file per day: `output.20251112.jsonl`, `output.20251113.jsonl`
- Flush every 30s OR 10MB
- **Use when**: Daily reports, aggregated data

---

### Pattern 4: Aggressive Flushing (Low Memory)

```bash
./retrieve_kraken_live_data_level2 -p "BTC/USD" -d 10 --hourly -f 10 -m 5242880
```

- Flush every 10s OR 5MB
- Hourly files
- **Use when**: Memory-constrained, high update rate

---

### Pattern 5: Separate Files Per Symbol

```bash
./retrieve_kraken_live_data_level2 -p "BTC/USD,ETH/USD,SOL/USD" -d 10 --separate-files --hourly
```

- Creates separate files per symbol:
  - `orderbook_BTC_USD.20251112_10.jsonl`
  - `orderbook_ETH_USD.20251112_10.jsonl`
  - `orderbook_SOL_USD.20251112_10.jsonl`
- Each with independent buffering and flushing
- **Use when**: Need per-symbol analysis

---

### Pattern 6: Backward Compatible (No Flush)

```bash
./retrieve_kraken_live_data_level2 -p "BTC/USD" -d 10 -f 0 -m 0
```

- No periodic flushing
- Data written immediately (old behavior)
- **Use when**: Short tests, backward compatibility needed

---

## Filename Examples

### No Segmentation

```
Output: kraken_orderbook.jsonl
```

### Hourly Segmentation

```
kraken_orderbook.20251112_10.jsonl  (Nov 12, 2025, 10:00 UTC)
kraken_orderbook.20251112_11.jsonl  (Nov 12, 2025, 11:00 UTC)
kraken_orderbook.20251112_12.jsonl  (Nov 12, 2025, 12:00 UTC)
```

### Daily Segmentation

```
kraken_orderbook.20251112.jsonl     (Nov 12, 2025)
kraken_orderbook.20251113.jsonl     (Nov 13, 2025)
kraken_orderbook.20251114.jsonl     (Nov 14, 2025)
```

### Separate Files + Hourly

```
kraken_orderbook_BTC_USD.20251112_10.jsonl
kraken_orderbook_ETH_USD.20251112_10.jsonl
kraken_orderbook_SOL_USD.20251112_10.jsonl
```

---

## Memory Threshold Quick Reference

| Threshold | Order Book Records (~500KB each) | Example Use Case |
|-----------|----------------------------------|------------------|
| 1 MB | ~2 snapshots | Very low memory systems |
| 5 MB | ~10 snapshots | Low memory, high I/O tolerance |
| **10 MB** (default) | ~20 snapshots | Balanced (recommended) |
| 20 MB | ~40 snapshots | High memory available |
| 50 MB | ~100 snapshots | Very high memory |

**Note**: Order book record size varies by depth (10, 25, 100, 500, 1000 levels).

---

## Flush Interval Quick Reference

| Interval | Data Loss Risk | I/O Frequency | Example Use Case |
|----------|----------------|---------------|------------------|
| 5s | Very low | High | Critical data, high reliability |
| 15s | Low | Medium-high | Important data |
| **30s** (default) | Low | Medium | Balanced (recommended) |
| 60s | Medium | Low | Non-critical data |
| 0 (disabled) | High | N/A | Short tests only |

---

## Status Output Interpretation

### Without Segmentation

```
[STATUS] BTC/USD: 1 snapshots, 392 updates
```

- Shows snapshot count and update count per pair

### With Segmentation

```
[SEGMENT] Starting new file: kraken_orderbook.20251112_11.jsonl
[FLUSH] Wrote records to kraken_orderbook.20251112_11.jsonl
[STATUS] BTC/USD: 1 snapshots, 392 updates
```

- **[SEGMENT]**: New segment file created
- **[FLUSH]**: Data written to disk (first 3 times only)

---

## Common Calculations

### Estimate File Size (Hourly)

```
Records/hour = Pairs × Updates/sec × 3600
JSONL bytes ≈ Records/hour × 500 bytes/record (10-depth)

Example (10 pairs, 1 update/sec):
10 × 1 × 3600 = 36,000 records/hour
36,000 × 500 bytes = ~18 MB/hour
```

### Estimate Memory Usage

```
Memory = Buffered_records × ~500 bytes

Example (10MB threshold):
10,485,760 / 500 = ~21,000 records buffered
```

### Estimate Flush Frequency

```
If time trigger: Every flush_interval seconds
If memory trigger: threshold / (pairs × update_rate × record_size)

Example (10MB threshold, 10 pairs, 1 update/sec):
10,485,760 / (10 × 1 × 500) = ~2097 seconds = ~35 minutes
→ Time trigger wins (30s)
```

---

## Troubleshooting

### Problem: Flushes too frequent (high I/O)

**Solution**: Increase flush interval or memory threshold

```bash
# Increase to 60s and 20MB
./retrieve_kraken_live_data_level2 -p "..." -f 60 -m 20971520
```

---

### Problem: Memory usage too high

**Solution**: Decrease memory threshold or flush interval

```bash
# Decrease to 10s or 5MB
./retrieve_kraken_live_data_level2 -p "..." -f 10 -m 5242880
```

---

### Problem: Files too large (segmentation)

**Solution**: Switch from daily to hourly segmentation

```bash
# Use hourly instead of daily
./retrieve_kraken_live_data_level2 -p "..." --hourly
```

---

### Problem: Too many small files

**Solution**: Switch from hourly to daily or increase flush interval

```bash
# Use daily segmentation
./retrieve_kraken_live_data_level2 -p "..." --daily

# Or reduce flush frequency
./retrieve_kraken_live_data_level2 -p "..." --hourly -f 60
```

---

### Problem: Data loss on crash

**Solution**: Enable more aggressive flushing

```bash
# Flush every 5 seconds
./retrieve_kraken_live_data_level2 -p "..." -f 5
```

---

## Decision Matrix

**Choose based on your needs:**

| Scenario | Flush Interval | Memory Threshold | Segmentation |
|----------|----------------|------------------|--------------|
| Short test (<1 hour) | 30s | 10MB | None |
| Medium run (1-24 hours) | 30s | 10MB | None or Hourly |
| Long run (>1 day) | 30s | 10MB | **Hourly** |
| Daily reports | 60s | 20MB | **Daily** |
| Memory constrained | 10s | 5MB | Hourly |
| High reliability | 5s | 5MB | Hourly |
| Low I/O tolerance | 60s | 20MB | Daily |
| Many pairs (10+) | 15s | 10MB | Hourly |
| Separate analysis | 30s | 10MB | **--separate-files** |

---

## Examples by Use Case

### Use Case 1: Production Monitoring (Multiple Pairs)

```bash
./retrieve_kraken_live_data_level2 \
  -p "BTC/USD,ETH/USD,SOL/USD,XRP/USD,ADA/USD" \
  -d 25 \
  --separate-files \
  --hourly \
  -f 30 \
  -m 10485760
```

**Result**: Separate hourly files per symbol, manageable sizes.

---

### Use Case 2: Critical Data Collection

```bash
./retrieve_kraken_live_data_level2 \
  -p "BTC/USD,ETH/USD" \
  -d 10 \
  --hourly \
  -f 5 \
  -m 5242880
```

**Result**: Aggressive flushing, minimal data loss risk.

---

### Use Case 3: Daily Aggregation

```bash
./retrieve_kraken_live_data_level2 \
  -p "BTC/USD,ETH/USD,SOL/USD" \
  -d 100 \
  --daily \
  -f 60
```

**Result**: One file per day for daily reports.

---

### Use Case 4: Memory-Constrained System

```bash
./retrieve_kraken_live_data_level2 \
  -p "BTC/USD" \
  -d 10 \
  --hourly \
  -f 10 \
  -m 2097152  # 2MB
```

**Result**: Very low memory usage with frequent flushes.

---

### Use Case 5: Quick Test

```bash
./retrieve_kraken_live_data_level2 \
  -p "BTC/USD" \
  -d 10 \
  -f 0 \
  -m 0
```

**Result**: Immediate writes, no buffering (old behavior).

---

## Comparison with Level 1

| Feature | Level 1 (Ticker) | Level 2 (Order Book) |
|---------|------------------|----------------------|
| File format | CSV | JSONL |
| Record size | ~120 bytes | ~500 KB (10-depth) |
| Default flush | 30s, 10MB | 30s, 10MB |
| Segmentation | Hourly/Daily | Hourly/Daily |
| Multi-file | No | Yes (--separate-files) |
| Writer class | `KrakenWebSocketClientBase` | `JsonLinesWriter` |

---

**For detailed design decisions and technical specifications, see**: [LEVEL2_MEMORY_MANAGEMENT_AND_SEGMENTATION.md](LEVEL2_MEMORY_MANAGEMENT_AND_SEGMENTATION.md)
