import requests

url = "https://api.kraken.com/0/public/Ticker"
params = {"pair": "XBTUSD"}  # XBT = BTC on Kraken
response = requests.get(url, params=params)
print(response.json())
