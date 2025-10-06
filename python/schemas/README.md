# Kraken API Schemas

This directory contains JSON schema definitions for Kraken cryptocurrency exchange API data structures.

## Available Schemas

### WebSocket API v2

#### websocket_v2_ticker_schema.json
Schema for the Ticker (Level 1) WebSocket v2 channel.

**Source**: https://docs.kraken.com/api/docs/websocket-v2/ticker/

**Includes definitions for**:
- `SubscriptionRequest` - Request to subscribe to ticker channel
- `SubscriptionResponse` - Response confirming subscription
- `UnsubscribeRequest` - Request to unsubscribe from ticker channel
- `UnsubscribeResponse` - Response confirming unsubscription
- `TickerData` - Individual ticker data object with all market fields
- `TickerUpdate` - Snapshot/update messages with ticker data
- `Heartbeat` - Connection heartbeat messages

**Usage Example**:
```python
import json
import jsonschema

# Load schema
with open('schemas/websocket_v2_ticker_schema.json', 'r') as f:
    schema = json.load(f)

# Validate subscription request
request = {
    "method": "subscribe",
    "params": {
        "channel": "ticker",
        "symbol": ["BTC/USD", "ETH/USD"]
    }
}

# Validate against SubscriptionRequest definition
jsonschema.validate(request, schema['definitions']['SubscriptionRequest'])
```

## Schema Format

All schemas follow [JSON Schema Draft 7](https://json-schema.org/draft-07/schema) specification.

## Updating Schemas

When Kraken updates their API, schemas should be updated from the official documentation:
- WebSocket v2: https://docs.kraken.com/api/docs/websocket-v2/
- REST API: https://docs.kraken.com/api/docs/rest-api/

## Validation Libraries

**Python**:
- `jsonschema` - JSON Schema validator

**JavaScript/TypeScript**:
- `ajv` - Another JSON Schema Validator
- `@apidevtools/json-schema-ref-parser` - Resolve $ref pointers

**Go**:
- `github.com/xeipuuv/gojsonschema`
