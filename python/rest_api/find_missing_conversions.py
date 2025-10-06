import pandas as pd
import json

# Load the generated CSV
df = pd.read_csv("../../data/kraken_top500_usd_volume.csv")

# Find pairs where quote_volume and usd_volume are equal (within small tolerance)
# This indicates the conversion factor was 1.0 (missing conversion)
tolerance = 0.01  # 1% tolerance for floating point comparison

missing_conversions = set()

for _, row in df.iterrows():
    pair = row['pair']
    quote_vol = row['quote_volume_24h']
    usd_vol = row['usd_volume_24h']

    # Skip if volumes are zero
    if quote_vol == 0 or usd_vol == 0:
        continue

    # Check if they're approximately equal (ratio close to 1.0)
    ratio = abs(usd_vol / quote_vol - 1.0)

    if ratio < tolerance:
        # Extract quote currency from pair
        if '/' in pair:
            parts = pair.split('/')
            if len(parts) == 2:
                quote_currency = parts[1]
                # Exclude currencies that are legitimately ~1 USD (stablecoins)
                usd_pegged = ['USD', 'USDT', 'USDC', 'DAI', 'PYUSD', 'RLUSD',
                              'USD1', 'USDD', 'USDQ', 'USDR']
                if quote_currency not in usd_pegged:
                    missing_conversions.add(quote_currency)
                    print(f"Missing conversion for {quote_currency} in pair {pair}")

print(f"\nFound {len(missing_conversions)} missing currency conversions:")
print(sorted(missing_conversions))

# Save to JSON file for later use
output = {
    "missing_currencies": sorted(missing_conversions),
    "note": "These currencies need conversion rates added to build_usd_conversion()"
}

with open("../../data/missing_conversions.json", "w") as f:
    json.dump(output, f, indent=2)

print("\nSaved missing currencies to ../../data/missing_conversions.json")
