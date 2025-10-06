import asyncio
import websockets
import json
import pandas as pd
from datetime import datetime

# List to store all ticker updates
ticker_history = []

async def kraken_ws(pairs):
    url = "wss://ws.kraken.com"
    async with websockets.connect(url) as ws:
        # Subscribe to ticker channel
        subscribe_msg = {
            "event": "subscribe",
            "pair": pairs,
            "subscription": {"name": "ticker"}
        }
        await ws.send(json.dumps(subscribe_msg))
        print(f"Subscribed to: {pairs}")

        while True:
            msg = await ws.recv()
            data = json.loads(msg)

            # Ignore system events
            if isinstance(data, dict):
                if data.get("event") in ["heartbeat", "systemStatus", "subscriptionStatus"]:
                    continue

            # Ticker messages
            if isinstance(data, list) and len(data) >= 4:
                ticker_data = data[1]
                pair = data[3]
                timestamp = datetime.utcnow()

                # Flatten the ticker data into a dict
                record = {
                    "timestamp": timestamp,
                    "pair": pair,
                    "ask_price": float(ticker_data['a'][0]),
                    "ask_whole_lot_volume": float(ticker_data['a'][1]),
                    "ask_lot_volume": float(ticker_data['a'][2]),
                    "bid_price": float(ticker_data['b'][0]),
                    "bid_whole_lot_volume": float(ticker_data['b'][1]),
                    "bid_lot_volume": float(ticker_data['b'][2]),
                    "last_trade_price": float(ticker_data['c'][0]),
                    "last_trade_volume": float(ticker_data['c'][1]),
                    "volume_today": float(ticker_data['v'][0]),
                    "volume_24h": float(ticker_data['v'][1]),
                    "vwap_today": float(ticker_data['p'][0]),
                    "vwap_24h": float(ticker_data['p'][1]),
                    "trades_today": int(ticker_data['t'][0]),
                    "trades_24h": int(ticker_data['t'][1]),
                    "low_today": float(ticker_data['l'][0]),
                    "low_24h": float(ticker_data['l'][1]),
                    "high_today": float(ticker_data['h'][0]),
                    "high_24h": float(ticker_data['h'][1]),
                    "opening_price": float(ticker_data['o'][1])
                }

                ticker_history.append(record)
                print(f"{timestamp} | {pair} | last: {record['last_trade_price']}")

if __name__ == "__main__":
    pairs = ["XBT/USD", "ETH/USD", "SOL/USD"]
    try:
        asyncio.run(kraken_ws(pairs))
    except KeyboardInterrupt:
        print("\nStopping and saving data to DataFrame...")
        df = pd.DataFrame(ticker_history)
        print(df.head())
        # Optional: save to CSV
        df.to_csv("../../data/kraken_ticker_history.csv", index=False)
        print("Saved to ../../data/kraken_ticker_history.csv")
