#pragma once

#include <string>
#include <string_view>
#include <vector>
#include <cstdint>
#include "market_event.hpp"

namespace backtest {

class CsvDataLoader {
public:
    explicit CsvDataLoader(const std::string& filepath);

    const std::vector<MarketEvent>& load();

    size_t eventCount() const noexcept;
    int64_t startTime() const noexcept;
    int64_t endTime() const noexcept;

private:
    bool parseLine(std::string_view line, MarketEvent& out);

    static int64_t priceToTicks(double price) noexcept;

    static Side parseSide(std::string_view token) noexcept;

    std::string filepath_;
    std::vector<MarketEvent> events_;
    bool loaded_ = false;
};

}