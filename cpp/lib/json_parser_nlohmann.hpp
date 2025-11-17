#ifndef JSON_PARSER_NLOHMANN_HPP
#define JSON_PARSER_NLOHMANN_HPP

#include <string>
#include <vector>
#include <functional>
#include <nlohmann/json.hpp>
#include "kraken_common.hpp"

namespace kraken {

/**
 * JSON parser adapter for nlohmann/json library
 *
 * Provides static methods for the template-based WebSocket client
 */
struct NlohmannJsonParser {
    using json = nlohmann::json;

    static const char* name() { return "nlohmann/json"; }

    static std::string build_subscription(const std::vector<std::string>& symbols) {
        json subscribe_msg = {
            {"method", "subscribe"},
            {"params", {
                {"channel", "ticker"},
                {"symbol", symbols},
                {"snapshot", true}
            }}
        };
        return subscribe_msg.dump();
    }

    static std::string build_unsubscribe(const std::vector<std::string>& symbols) {
        json unsubscribe_msg = {
            {"method", "unsubscribe"},
            {"params", {
                {"channel", "ticker"},
                {"symbol", symbols}
            }}
        };
        return unsubscribe_msg.dump();
    }

    static void parse_message(const std::string& payload,
                              std::function<void(const TickerRecord&)> callback) {
        json data = json::parse(payload);

        // Handle subscription status
        if (data.contains("method") && data["method"] == "subscribe") {
            if (data.contains("success") && data["success"].get<bool>()) {
                std::cout << "Successfully subscribed" << std::endl;
            } else {
                std::cerr << "Subscription failed: " << data.dump() << std::endl;
            }
            return;
        }

        // Handle heartbeat
        if (data.contains("channel") && data["channel"] == "heartbeat") {
            return;
        }

        // Handle ticker messages
        if (data.contains("channel") && data["channel"] == "ticker") {
            if (!data.contains("type") || !data.contains("data")) {
                return;
            }

            std::string type = data["type"].get<std::string>();
            if (type != "snapshot" && type != "update") {
                return;
            }

            std::string timestamp = Utils::get_utc_timestamp();

            for (const auto& ticker : data["data"]) {
                TickerRecord record;
                record.timestamp = timestamp;

                // Extract fields
                if (ticker.contains("symbol")) {
                    record.pair = ticker["symbol"].get<std::string>();
                }

                record.type = type;

                if (ticker.contains("bid")) record.bid = ticker["bid"].get<double>();
                if (ticker.contains("bid_qty")) record.bid_qty = ticker["bid_qty"].get<double>();
                if (ticker.contains("ask")) record.ask = ticker["ask"].get<double>();
                if (ticker.contains("ask_qty")) record.ask_qty = ticker["ask_qty"].get<double>();
                if (ticker.contains("last")) record.last = ticker["last"].get<double>();
                if (ticker.contains("volume")) record.volume = ticker["volume"].get<double>();
                if (ticker.contains("vwap")) record.vwap = ticker["vwap"].get<double>();
                if (ticker.contains("low")) record.low = ticker["low"].get<double>();
                if (ticker.contains("high")) record.high = ticker["high"].get<double>();
                if (ticker.contains("change")) record.change = ticker["change"].get<double>();
                if (ticker.contains("change_pct")) record.change_pct = ticker["change_pct"].get<double>();

                callback(record);
            }
        }
    }
};

} // namespace kraken

#endif // JSON_PARSER_NLOHMANN_HPP
