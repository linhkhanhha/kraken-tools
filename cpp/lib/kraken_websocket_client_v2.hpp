#ifndef KRAKEN_WEBSOCKET_CLIENT_V2_HPP
#define KRAKEN_WEBSOCKET_CLIENT_V2_HPP

#include "kraken_websocket_client_base.hpp"
#include "json_parser_nlohmann.hpp"

namespace kraken {

/**
 * WebSocket client using nlohmann/json parser
 *
 * This is a simple typedef using the template base class.
 * All implementation is in kraken_websocket_client_base.hpp
 */
using KrakenWebSocketClientV2 = KrakenWebSocketClientBase<NlohmannJsonParser>;

} // namespace kraken

#endif // KRAKEN_WEBSOCKET_CLIENT_V2_HPP
