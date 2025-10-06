#!/usr/bin/env python3

# Import WebSocket client library (and others)
import sys
import json
import signal
import time
import _thread
import websocket

# Parse command line arguments (symbol and depth)
if len(sys.argv) < 3:
    sys.exit(1)
else:
    api_symbol = sys.argv[1]
    api_depth = int(sys.argv[2])

# Define order book variables
api_book = {'bid': {}, 'ask': {}}

# Define order book update functions
def dicttofloat(data):
    return float(data[0])

def api_book_update(api_book_side, api_book_data):
    for data in api_book_data:
        price_level = data[0]
        volume = data[1]
        if float(volume) > 0.0:
            api_book[api_book_side][price_level] = volume
        else:
            # remove level when volume is zero
            api_book[api_book_side].pop(price_level, None)

    if api_book_side == 'bid':
        api_book['bid'] = dict(sorted(api_book['bid'].items(), key=dicttofloat, reverse=True)[:api_depth])
    elif api_book_side == 'ask':
        api_book['ask'] = dict(sorted(api_book['ask'].items(), key=dicttofloat)[:api_depth])

# Define WebSocket callback functions
def ws_thread(*args):
    ws = websocket.WebSocketApp('wss://ws.kraken.com/', on_open=ws_open, on_message=ws_message)
    ws.run_forever()

def ws_open(ws):
    ws.send('{"event":"subscribe", "subscription":{"name":"book", "depth":%(api_depth)d}, "pair":["%(api_symbol)s"]}' % {'api_depth': api_depth, 'api_symbol': api_symbol})

def ws_message(ws, ws_data):
    api_data = json.loads(ws_data)
    if 'event' in api_data:
        return
    else:
        # initial snapshot (as / bs) or incremental updates (a / b)
        if 'as' in api_data[1]:
            api_book_update('ask', api_data[1]['as'])
            api_book_update('bid', api_data[1]['bs'])
        else:
            for data in api_data[1:len(api_data)-2]:
                if 'a' in data:
                    api_book_update('ask', data['a'])
                elif 'b' in data:
                    api_book_update('bid', data['b'])

# Start new thread for WebSocket interface
_thread.start_new_thread(ws_thread, ())

# Output order book (once per second) in main thread
try:
    while True:
        if len(api_book['bid']) < api_depth or len(api_book['ask']) < api_depth:
            time.sleep(1)
        else:
            bid = sorted(api_book['bid'].items(), key=dicttofloat, reverse=True)
            ask = sorted(api_book['ask'].items(), key=dicttofloat)
            print('Bid\t\t\t\t\t\t\tAsk')
            for count in range(api_depth):
                print('%(bidprice)s (%(bidvolume)s)\t\t\t\t%(askprice)s (%(askvolume)s)' \
                      % {'bidprice': bid[count][0], 'bidvolume': bid[count][1], 'askprice': ask[count][0], 'askvolume': ask[count][1]})
            time.sleep(1)
except KeyboardInterrupt:
    sys.exit(0)
