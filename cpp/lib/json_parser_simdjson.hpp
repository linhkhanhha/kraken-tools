#ifndef JSON_PARSER_SIMDJSON_HPP
#define JSON_PARSER_SIMDJSON_HPP

#include <string>
#include <vector>
#include <functional>
#include <sstream>
#include <simdjson.h>
#include "kraken_common.hpp"

namespace kraken {

/**
 * JSON parser adapter for simdjson library
 *
 * Provides static methods for the template-based WebSocket client
 */
struct SimdjsonParser {
    static const char* name() { return "simdjson"; }

    static std::string build_subscription(const std::vector<std::string>& symbols) {
        // simdjson is read-only, so build JSON manually
        std::ostringstream subscribe_msg;
        subscribe_msg << R"({"method":"subscribe","params":{)";
        subscribe_msg << R"("channel":"ticker",)";
        subscribe_msg << R"("symbol":[)";

        for (size_t i = 0; i < symbols.size(); ++i) {
            if (i > 0) subscribe_msg << ",";
            subscribe_msg << "\"" << symbols[i] << "\"";
        }

        subscribe_msg << R"(],"snapshot":true}})";
        return subscribe_msg.str();
    }

    static void parse_message(const std::string& payload,
                              std::function<void(const TickerRecord&)> callback) {
        try {
            // simdjson on-demand parsing
            simdjson::ondemand::parser parser;
            simdjson::padded_string padded(payload);
            simdjson::ondemand::document doc = parser.iterate(padded);

            // Handle subscription status
            if (auto method_result = doc["method"]; !method_result.error()) {
                std::string_view method = method_result.value();
                if (method == "subscribe") {
                    if (auto success_result = doc["success"]; !success_result.error()) {
                        bool success = success_result.value();
                        if (success) {
                            std::cout << "Successfully subscribed (simdjson)" << std::endl;
                        } else {
                            std::cerr << "Subscription failed" << std::endl;
                        }
                    }
                    return;
                }
            }

            // Handle heartbeat
            if (auto channel_result = doc["channel"]; !channel_result.error()) {
                std::string_view channel = channel_result.value();
                if (channel == "heartbeat") {
                    return;
                }

                // Handle ticker messages
                if (channel == "ticker") {
                    auto type_result = doc["type"];
                    if (type_result.error()) return;

                    std::string_view type_str = type_result.value();
                    if (type_str != "snapshot" && type_str != "update") return;

                    std::string timestamp = Utils::get_utc_timestamp();

                    // Parse data array
                    auto data_result = doc["data"];
                    if (data_result.error()) return;

                    simdjson::ondemand::array data_array = data_result.value();

                    for (auto ticker_value : data_array) {
                        simdjson::ondemand::object ticker = ticker_value.get_object();

                        TickerRecord record;
                        record.timestamp = timestamp;

                        // Extract string fields
                        if (auto symbol = ticker["symbol"]; !symbol.error()) {
                            std::string_view sv = symbol.value();
                            record.pair = std::string(sv);
                        }

                        record.type = std::string(type_str);

                        // Extract double fields
                        if (auto bid = ticker["bid"]; !bid.error()) {
                            record.bid = bid.get_double();
                        }
                        if (auto bid_qty = ticker["bid_qty"]; !bid_qty.error()) {
                            record.bid_qty = bid_qty.get_double();
                        }
                        if (auto ask = ticker["ask"]; !ask.error()) {
                            record.ask = ask.get_double();
                        }
                        if (auto ask_qty = ticker["ask_qty"]; !ask_qty.error()) {
                            record.ask_qty = ask_qty.get_double();
                        }
                        if (auto last = ticker["last"]; !last.error()) {
                            record.last = last.get_double();
                        }
                        if (auto volume = ticker["volume"]; !volume.error()) {
                            record.volume = volume.get_double();
                        }
                        if (auto vwap = ticker["vwap"]; !vwap.error()) {
                            record.vwap = vwap.get_double();
                        }
                        if (auto low = ticker["low"]; !low.error()) {
                            record.low = low.get_double();
                        }
                        if (auto high = ticker["high"]; !high.error()) {
                            record.high = high.get_double();
                        }
                        if (auto change = ticker["change"]; !change.error()) {
                            record.change = change.get_double();
                        }
                        if (auto change_pct = ticker["change_pct"]; !change_pct.error()) {
                            record.change_pct = change_pct.get_double();
                        }

                        callback(record);
                    }
                }
            }

        } catch (const simdjson::simdjson_error& e) {
            std::cerr << "simdjson parsing error: " << simdjson::error_message(e.error()) << std::endl;
        }
    }
};

} // namespace kraken

#endif // JSON_PARSER_SIMDJSON_HPP
