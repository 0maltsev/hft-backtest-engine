#include "backtest/csv_data_loader.hpp"
#include <iostream>

int main() {
    backtest::CsvDataLoader loader("MD/trades.csv");

    const auto& events = loader.load();

    std::cout << "sizeof(MarketEvent): " << sizeof(backtest::MarketEvent) << "\n";
    std::cout << "alignof(MarketEvent): " << alignof(backtest::MarketEvent) << "\n";

    std::cout << "Loaded " << events.size() << " events\n";
    if (!events.empty()) {
        std::cout << "First event: ts=" << events[0].timestamp_us 
                << ", price_ticks=" << events[0].price_ticks << "\n";
    }

    for (size_t i = 0; i < std::min(events.size(), size_t(5)); ++i) {
        double price_debug = static_cast<double>(events[i].price_ticks) / 10'000'000;
        std::cout << "[" << i << "] " << events[i].timestamp_us 
                << " | " << (events[i].side == backtest::Side::Buy ? "BUY" : "SELL")
                << " @ " << price_debug << " x " << events[i].amount << "\n";
    }

    return 0;
}