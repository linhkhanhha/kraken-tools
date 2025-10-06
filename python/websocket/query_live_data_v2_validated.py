import asyncio
import websockets
import json
import pandas as pd
from datetime import datetime
import jsonschema

# Load schema for validation
with open('../schemas/websocket_v2_ticker_schema.json', 'r') as f:
    schema = json.load(f)

# List to store all ticker updates
ticker_history = []

async def kraken_ws_v2(pairs):
    url = "wss://ws.kraken.com/v2"
    async with websockets.connect(url) as ws:
        # Build subscription request
        subscribe_msg = {
            "method": "subscribe",
            "params": {
                "channel": "ticker",
                "symbol": pairs,
                "snapshot": True
            }
        }

        # Validate subscription request before sending
        try:
            jsonschema.validate(subscribe_msg, schema['definitions']['SubscriptionRequest'])
            print("✓ Subscription request validated")
        except jsonschema.ValidationError as e:
            print(f"✗ Invalid subscription request: {e.message}")
            return

        await ws.send(json.dumps(subscribe_msg))
        print(f"Subscribed to: {pairs}")

        while True:
            msg = await ws.recv()
            data = json.loads(msg)

            # Handle subscription status messages
            if data.get("method") == "subscribe":
                # Validate subscription response
                try:
                    jsonschema.validate(data, schema['definitions']['SubscriptionResponse'])
                    print("✓ Subscription response validated")
                except jsonschema.ValidationError as e:
                    print(f"✗ Invalid subscription response: {e.message}")

                if data.get("success"):
                    print(f"Successfully subscribed: {data}")
                else:
                    print(f"Subscription failed: {data}")
                continue

            # Handle heartbeat
            if data.get("channel") == "heartbeat":
                try:
                    jsonschema.validate(data, schema['definitions']['Heartbeat'])
                except jsonschema.ValidationError as e:
                    print(f"✗ Invalid heartbeat: {e.message}")
                continue

            # Handle ticker messages
            if data.get("channel") == "ticker" and data.get("type") in ["snapshot", "update"]:
                # Validate ticker update
                try:
                    jsonschema.validate(data, schema['definitions']['TickerUpdate'])
                except jsonschema.ValidationError as e:
                    print(f"✗ Invalid ticker update: {e.message}")
                    print(f"Message: {json.dumps(data, indent=2)}")
                    continue

                timestamp = datetime.utcnow()

                # v2 returns data as an array of ticker objects
                for ticker_data in data.get("data", []):
                    symbol = ticker_data.get("symbol")

                    # Build record with v2 field names
                    record = {
                        "timestamp": timestamp,
                        "pair": symbol,
                        "type": data.get("type"),  # "snapshot" or "update"
                        "bid": float(ticker_data.get("bid", 0)),
                        "bid_qty": float(ticker_data.get("bid_qty", 0)),
                        "ask": float(ticker_data.get("ask", 0)),
                        "ask_qty": float(ticker_data.get("ask_qty", 0)),
                        "last": float(ticker_data.get("last", 0)),
                        "volume": float(ticker_data.get("volume", 0)),
                        "vwap": float(ticker_data.get("vwap", 0)),
                        "low": float(ticker_data.get("low", 0)),
                        "high": float(ticker_data.get("high", 0)),
                        "change": float(ticker_data.get("change", 0)),
                        "change_pct": float(ticker_data.get("change_pct", 0))
                    }

                    ticker_history.append(record)
                    print(f"{timestamp} | {symbol} | last: {record['last']} | change: {record['change_pct']:.2f}%")

if __name__ == "__main__":
    pairs = ["BTC/USD", "ETH/USD", "SOL/USD"]
    try:
        asyncio.run(kraken_ws_v2(pairs))
    except KeyboardInterrupt:
        print("\nStopping and saving data to DataFrame...")
        df = pd.DataFrame(ticker_history)
        print(df.head())
        # Optional: save to CSV
        df.to_csv("../../data/kraken_ticker_history_v2.csv", index=False)
        print("Saved to ../../data/kraken_ticker_history_v2.csv")
