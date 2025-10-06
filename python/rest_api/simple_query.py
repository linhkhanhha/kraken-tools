import krakenex

api = krakenex.API()
api.load_key('/export1/rocky/.kraken/kraken.key')  # File with API_KEY and API_SECRET on two lines

# Query account balance
balance = api.query_private('Balance')
print(balance)
