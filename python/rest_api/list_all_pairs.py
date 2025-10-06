import requests

url = "https://api.kraken.com/0/public/AssetPairs"
response = requests.get(url)
data = response.json()

if data["error"]:
    print("Error:", data["error"])
else:
    pairs = list(data["result"].keys())
    print(f"Total pairs: {len(pairs)}")
    ws_pairs = [info['wsname'] for info in data["result"].values() if 'wsname' in info]
    print(ws_pairs[:20])
