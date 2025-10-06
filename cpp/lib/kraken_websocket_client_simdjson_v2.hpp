#ifndef KRAKEN_WEBSOCKET_CLIENT_SIMDJSON_V2_HPP
#define KRAKEN_WEBSOCKET_CLIENT_SIMDJSON_V2_HPP

#include "kraken_websocket_client_base.hpp"
#include "json_parser_simdjson.hpp"

namespace kraken {

/**
 * WebSocket client using simdjson parser (2-5x faster)
 *
 * This is a simple typedef using the template base class.
 * All implementation is in kraken_websocket_client_base.hpp
 */
using KrakenWebSocketClientSimdjsonV2 = KrakenWebSocketClientBase<SimdjsonParser>;

} // namespace kraken

#endif // KRAKEN_WEBSOCKET_CLIENT_SIMDJSON_V2_HPP
