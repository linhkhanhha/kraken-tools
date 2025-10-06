#ifndef KRAKEN_WEBSOCKET_CLIENT_SIMDJSON_V2_HYBRID_HPP
#define KRAKEN_WEBSOCKET_CLIENT_SIMDJSON_V2_HYBRID_HPP

#include "kraken_websocket_client_base_hybrid.hpp"
#include "json_parser_simdjson.hpp"

namespace kraken {

/**
 * HYBRID PERFORMANCE WebSocket client using simdjson parser
 *
 * Usage Modes:
 *
 * 1. DEFAULT MODE (easy, std::function):
 *    KrakenWebSocketClientSimdjsonV2Hybrid client;
 *    client.set_update_callback([](const TickerRecord& r) { ... });
 *
 * 2. PERFORMANCE MODE (zero overhead, template):
 *    auto callback = [](const TickerRecord& r) { ... };
 *    KrakenWebSocketClientSimdjsonV2Hybrid<decltype(callback)> client;
 *    client.set_update_callback(callback);
 *
 * Performance difference:
 * - Default mode: ~5-10ns overhead per callback (std::function)
 * - Performance mode: ~0ns overhead (fully inlined template)
 *
 * For high-frequency updates (>10K/sec), performance mode provides 5-10x
 * faster callback invocation through compiler inlining.
 */
template<typename UpdateCallback = std::function<void(const TickerRecord&)>>
using KrakenWebSocketClientSimdjsonV2Hybrid =
    KrakenWebSocketClientBaseHybrid<SimdjsonParser, UpdateCallback>;

} // namespace kraken

#endif // KRAKEN_WEBSOCKET_CLIENT_SIMDJSON_V2_HYBRID_HPP
