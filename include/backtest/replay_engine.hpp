#pragma once
#include "backtest/csv_data_loader.hpp"
#include "backtest/strategy.hpp"
#include <vector>
#include <chrono>

namespace backtest {

template<Strategy StrategyType>
class ReplayEngine {
public:
    ReplayEngine(const std::vector<MarketEvent>& events, StrategyType& strategy)
        : events_(events), strategy_(strategy) {}

    void run() {
        auto start_time = std::chrono::high_resolution_clock::now();
        strategy_.on_init();
        
        for (const auto& event : events_) {
            strategy_.on_event(event);
            ++processed_count_;
        }
        
        strategy_.on_finish();
        
        auto end_time = std::chrono::high_resolution_clock::now();
        elapsed_us_ = std::chrono::duration_cast<std::chrono::microseconds>(
            end_time - start_time).count();
    }

    // Геттеры статистики
    [[nodiscard]] size_t processedCount() const noexcept { return processed_count_; }
    [[nodiscard]] int64_t elapsedUs() const noexcept { return elapsed_us_; }
    [[nodiscard]] size_t totalEvents() const noexcept { return events_.size(); }

private:
    const std::vector<MarketEvent>& events_;
    StrategyType& strategy_;
    size_t processed_count_ = 0;
    int64_t elapsed_us_ = 0;
};

}