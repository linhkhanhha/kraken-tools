# Memory Management & Segmentation - Quick Reference

**Last Updated**: 2025-11-12

---

## CLI Options Summary

```bash
./retrieve_kraken_live_data_level1 [options]
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

### Pattern 1: Default (Recommended for Most Use Cases)
```bash
./retrieve_kraken_live_data_level1 -p "BTC/USD,ETH/USD"
```
- Flush every 30s OR when memory exceeds 10MB
- Single output file
- **Use when**: Short to medium duration runs (hours to days)

---

### Pattern 2: Hourly Segmentation (Recommended for Long-Running)
```bash
./retrieve_kraken_live_data_level1 -p "BTC/USD" --hourly
```
- One file per hour: `output.20251112_10.csv`, `output.20251112_11.csv`, ...
- Flush every 30s OR 10MB
- **Use when**: Multi-day collection, need manageable file sizes

---

### Pattern 3: Daily Segmentation
```bash
./retrieve_kraken_live_data_level1 -p "BTC/USD" --daily
```
- One file per day: `output.20251112.csv`, `output.20251113.csv`, ...
- Flush every 30s OR 10MB
- **Use when**: Daily reports, aggregated data

---

### Pattern 4: Aggressive Flushing (Low Memory)
```bash
./retrieve_kraken_live_data_level1 -p "BTC/USD" --hourly -f 10 -m 5242880
```
- Flush every 10s OR 5MB
- Hourly files
- **Use when**: Memory-constrained environment, high-frequency updates

---

### Pattern 5: Memory-Only Flush
```bash
./retrieve_kraken_live_data_level1 -p "BTC/USD" --hourly -f 0 -m 20971520
```
- Flush only when memory exceeds 20MB
- No time-based flush
- **Use when**: Bursty traffic, want to minimize I/O

---

### Pattern 6: Backward Compatible (No Flush)
```bash
./retrieve_kraken_live_data_level1 -p "BTC/USD" -f 0 -m 0
```
- No periodic flushing
- All data in memory until shutdown
- **Use when**: Short tests, backward compatibility needed

---

## Filename Examples

### No Segmentation
```
Output: kraken_ticker_live_level1.csv
```

### Hourly Segmentation
```
kraken_ticker_live_level1.20251112_10.csv  (Nov 12, 2025, 10:00 UTC)
kraken_ticker_live_level1.20251112_11.csv  (Nov 12, 2025, 11:00 UTC)
kraken_ticker_live_level1.20251112_12.csv  (Nov 12, 2025, 12:00 UTC)
...
```

### Daily Segmentation
```
kraken_ticker_live_level1.20251112.csv     (Nov 12, 2025)
kraken_ticker_live_level1.20251113.csv     (Nov 13, 2025)
kraken_ticker_live_level1.20251114.csv     (Nov 14, 2025)
...
```

### Custom Output Filename
```bash
./retrieve_kraken_live_data_level1 -p "BTC/USD" -o my_data.csv --hourly
```
Output:
```
my_data.20251112_10.csv
my_data.20251112_11.csv
...
```

---

## Memory Threshold Quick Reference

| Threshold | Records (~120 bytes each) | Example Use Case |
|-----------|--------------------------|------------------|
| 1 MB | ~8,000 | Very low memory systems |
| 5 MB | ~40,000 | Low memory, high I/O tolerance |
| **10 MB** (default) | ~80,000 | Balanced (recommended) |
| 20 MB | ~160,000 | High memory available |
| 50 MB | ~400,000 | Very high memory, minimize I/O |

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
[STATUS] Running time: 90s | Updates: 342 | Flushes: 3 | Memory: 0.5MB | Pending: 12
```
- **Running time**: Elapsed seconds since start
- **Updates**: Total ticker updates received
- **Flushes**: Number of flushes to disk
- **Memory**: Current buffer size before next flush
- **Pending**: Records in pending queue

### With Segmentation
```
[STATUS] Running time: 3700s | Updates: 15420 | Flushes: 123 | Memory: 0.5MB | Pending: 12
         Current file: output.20251112_10.csv (3 files created)
```
- **Current file**: Active segment file being written
- **Files created**: Total number of segment files created so far

---

## Common Calculations

### Estimate Records Per Day
```
Records/day = Pairs × Updates/sec × 86400
Example: 100 pairs × 1 update/sec × 86400 = 8,640,000 records/day
```

### Estimate Memory Usage
```
Memory = Records × ~120 bytes
Example: 80,000 records × 120 bytes = ~9.6 MB
```

### Estimate File Size (Hourly)
```
Records/hour = Pairs × Updates/sec × 3600
CSV bytes ≈ Records/hour × 150 bytes/record (with CSV overhead)

Example:
100 pairs × 1 update/sec × 3600 = 360,000 records/hour
360,000 × 150 bytes = 54 MB/hour
```

### Estimate Flush Frequency
```
If time trigger: Every flush_interval seconds
If memory trigger: memory_threshold / (pairs × update_rate × record_size)

Example (10MB threshold, 100 pairs, 1 update/sec):
10,485,760 / (100 × 1 × 120) = ~873 seconds = ~14.5 minutes
```

---

## Troubleshooting

### Problem: Flushes too frequent (high I/O)
**Solution**: Increase flush interval or memory threshold
```bash
# Increase to 60s and 20MB
./retrieve_kraken_live_data_level1 -p "..." -f 60 -m 20971520
```

---

### Problem: Memory usage too high
**Solution**: Decrease memory threshold or flush interval
```bash
# Decrease to 10s or 5MB
./retrieve_kraken_live_data_level1 -p "..." -f 10 -m 5242880
```

---

### Problem: Files too large (segmentation)
**Solution**: Switch from daily to hourly segmentation
```bash
# Use hourly instead of daily
./retrieve_kraken_live_data_level1 -p "..." --hourly
```

---

### Problem: Too many small files
**Solution**: Switch from hourly to daily or increase flush interval
```bash
# Use daily segmentation
./retrieve_kraken_live_data_level1 -p "..." --daily

# Or reduce flush frequency
./retrieve_kraken_live_data_level1 -p "..." --hourly -f 60
```

---

### Problem: Data loss on crash
**Solution**: Enable more aggressive flushing
```bash
# Flush every 5 seconds
./retrieve_kraken_live_data_level1 -p "..." -f 5
```

---

## API Reference (Library Usage)

### Configure Flush Parameters
```cpp
KrakenWebSocketClientSimdjsonV2 client;

// Set flush interval (0 to disable time-based flush)
client.set_flush_interval(std::chrono::seconds(30));

// Set memory threshold (0 to disable memory-based flush)
client.set_memory_threshold(10 * 1024 * 1024);  // 10 MB

// Set output file
client.set_output_file("output.csv");
```

### Configure Segmentation
```cpp
// Enable hourly segmentation
client.set_segment_mode(kraken::SegmentMode::HOURLY);

// Enable daily segmentation
client.set_segment_mode(kraken::SegmentMode::DAILY);

// Disable segmentation (default)
client.set_segment_mode(kraken::SegmentMode::NONE);
```

### Get Statistics
```cpp
// Get flush count
size_t flushes = client.get_flush_count();

// Get current memory usage
size_t memory = client.get_current_memory_usage();

// Get current segment filename
std::string filename = client.get_current_segment_filename();

// Get segment count
size_t segments = client.get_segment_count();
```

---

## Log Messages Reference

### Flush Messages
```
[FLUSH] Wrote 234 records to output.csv
```
- Appears on first 3 flushes (quiet mode after that)
- Shows number of records written

### Segment Messages
```
[SEGMENT] Starting new file: output.20251112_11.csv
```
- Appears when hour/day boundary crossed
- Shows new segment filename

### Status Messages
```
[STATUS] Running time: 90s | Updates: 342 | Flushes: 3 | Memory: 0.5MB | Pending: 12
```
- Appears every 30 seconds
- Shows current operational statistics

---

## Performance Impact

| Feature | CPU Impact | Memory Impact | I/O Impact |
|---------|-----------|---------------|------------|
| Periodic Flush | Negligible | Low (bounded) | Low (append mode) |
| Hourly Segmentation | Negligible | None | None (uses flush) |
| Daily Segmentation | Negligible | None | None (uses flush) |

**Typical Overhead**:
- Flush operation: 5-50ms (depends on disk)
- Segment detection: <1ms
- Filename generation: <1ms

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
| Many pairs (100+) | 15s | 10MB | Hourly |

---

## Examples by Use Case

### Use Case 1: Production Monitoring (100 pairs)
```bash
./retrieve_kraken_live_data_level1 \
  -p "@top100.csv:pair:100" \
  --hourly \
  -f 30 \
  -m 10485760
```

### Use Case 2: Critical Data Collection
```bash
./retrieve_kraken_live_data_level1 \
  -p "BTC/USD,ETH/USD" \
  --hourly \
  -f 5 \
  -m 5242880
```

### Use Case 3: Daily Aggregation
```bash
./retrieve_kraken_live_data_level1 \
  -p "BTC/USD,ETH/USD,SOL/USD" \
  --daily \
  -f 60
```

### Use Case 4: Memory-Constrained System
```bash
./retrieve_kraken_live_data_level1 \
  -p "BTC/USD" \
  --hourly \
  -f 10 \
  -m 2097152  # 2MB
```

### Use Case 5: Quick Test
```bash
./retrieve_kraken_live_data_level1 \
  -p "BTC/USD" \
  -f 0 \
  -m 0
```

---

**For detailed design decisions and technical specifications, see**: [MEMORY_MANAGEMENT_AND_SEGMENTATION.md](MEMORY_MANAGEMENT_AND_SEGMENTATION.md)
