import requests
import pandas as pd
import time

ASSET_PAIRS_URL = "https://api.kraken.com/0/public/AssetPairs"
TICKER_URL = "https://api.kraken.com/0/public/Ticker"

# Step 1: Fetch all asset pairs
def get_asset_pairs(session):
    r = session.get(ASSET_PAIRS_URL, timeout=10)
    r.raise_for_status()
    j = r.json()
    if j.get("error"):
        raise RuntimeError("AssetPairs error: " + str(j["error"]))
    return j["result"]

# Step 2: Chunked ticker fetch
def chunked_ticker(session, pair_list, chunk_size=30, pause=0.2):
    results = {}
    for start in range(0, len(pair_list), chunk_size):
        chunk = pair_list[start:start+chunk_size]
        params = {"pair": ",".join(chunk)}
        resp = session.get(TICKER_URL, params=params, timeout=15)
        resp.raise_for_status()
        j = resp.json()
        if j.get("error"):
            print("Error fetching chunk:", j["error"])
            continue
        results.update(j.get("result", {}))
        time.sleep(pause)
    return results

# Step 3: Build conversion map to USD
def build_usd_conversion(ticker_results, api_to_ws):
    """
    Build approximate USD conversion for common quote currencies.
    For simplicity:
      - USD = 1
      - USDT/USDC = 1
      - EUR/GBP/JPY = use last price from ticker if available
      - BTC/ETH = use BTC/USD or ETH/USD last price

    Automatically finds all XXX/USD pairs and adds them to conversion map.
    For fiat pairs like USD/JPY, inverts the price to get JPY/USD conversion.
    """
    # USD-pegged stablecoins (approximately 1:1 with USD)
    conversion = {
        "USD": 1.0, "USDT": 1.0, "USDC": 1.0, "DAI": 1.0,
        "PYUSD": 1.0, "RLUSD": 1.0, "USD1": 1.0, "USDD": 1.0,
        "USDQ": 1.0, "USDR": 1.0
    }

    # Scan all ticker results for pairs with USD
    for api_name, ticker_data in ticker_results.items():
        wsname = api_to_ws.get(api_name, api_name)
        if '/' in wsname:
            parts = wsname.split('/')
            if len(parts) == 2:
                base_currency = parts[0]
                quote_currency = parts[1]

                try:
                    price = float(ticker_data['c'][0])
                    if price <= 0:
                        continue

                    # If quote is USD, base currency price is already in USD
                    if quote_currency == 'USD':
                        conversion[base_currency] = price
                    # If base is USD, invert to get quote currency conversion
                    elif base_currency == 'USD':
                        conversion[quote_currency] = 1.0 / price

                except (KeyError, ValueError, IndexError) as e:
                    print(f"Warning: Could not get price for {wsname}: {e}")

    return conversion

# Step 4: Calculate USD notional volumes
def calculate_usd_volume(ticker_results, api_to_ws, conversion_map):
    rows = []
    for api_name, info in ticker_results.items():
        try:
            wsname = api_to_ws.get(api_name, api_name)
            base_vol = float(info['v'][1])  # 24h base volume
            price = float(info['c'][0])     # last price in quote currency

            # Determine quote currency from api_name
            quote_currency = None
            for k, v in api_to_ws.items():
                if k == api_name:
                    # wsname like "ETH/USD" → split
                    parts = v.split("/")
                    if len(parts) == 2:
                        quote_currency = parts[1]
                    break

            quote_vol = base_vol * price  # quote volume in quote currency

            if quote_currency is None:
                usd_vol = quote_vol  # fallback
            else:
                factor = conversion_map.get(quote_currency, 1.0)
                usd_vol = quote_vol * factor

            rows.append({
                "pair": wsname,
                "base_volume_24h": base_vol,
                "quote_volume_24h": quote_vol,
                "usd_volume_24h": usd_vol
            })
        except Exception as e:
            print(f"Skipping {api_name}: {e}")
            continue
    df = pd.DataFrame(rows)
    df.sort_values("usd_volume_24h", ascending=False, inplace=True)
    return df

# ===== Main =====
if __name__ == "__main__":
    session = requests.Session()

    # 1. Get asset pairs
    pairs_info = get_asset_pairs(session)
    api_to_ws = {api_name: info['wsname'] for api_name, info in pairs_info.items() if 'wsname' in info}
    pair_list = list(api_to_ws.keys())
    print(f"Total tradeable pairs: {len(pair_list)}")

    # 2. Fetch ticker info
    ticker_results = chunked_ticker(session, pair_list, chunk_size=30)

    # 3. Build conversion map
    conversion_map = build_usd_conversion(ticker_results, api_to_ws)
    print("USD conversion map:", conversion_map)

    # 4. Calculate USD notional volumes
    df = calculate_usd_volume(ticker_results, api_to_ws, conversion_map)

    # 5. Top 500 pairs
    top500 = df.head(500)
    print(top500.head(20))

    # 6. Save to CSV
    df.to_csv("../../data/kraken_top500_usd_volume.csv", index=False)
    print("Saved top 500 USD volume pairs to ../../data/kraken_top500_usd_volume.csv")
