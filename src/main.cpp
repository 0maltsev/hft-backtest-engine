#include "backtest/csv_data_loader.hpp"
#include "backtest/replay_engine.hpp"
#include "backtest/test_strategy.hpp"
#include <iostream>


int main() {
    try {
        // Загрузка
        backtest::CsvDataLoader loader("MD/trades.csv");
        const auto& events = loader.load();
        
        std::cout << "Loaded " << events.size() << " events\n";
        std::cout << "sizeof(MarketEvent): " << sizeof(backtest::MarketEvent) << " bytes\n";
        std::cout << "alignof(MarketEvent): " << alignof(backtest::MarketEvent) << " bytes\n\n";
        
        // Реплей
        backtest::TestStrategy strategy;
        backtest::ReplayEngine engine(events, strategy);
        
        std::cout << "Starting replay...\n";
        engine.run();
        
        // Статистика
        std::cout << "Processed: " << engine.processedCount() << " events\n";
        std::cout << "Elapsed: " << static_cast<double>(engine.elapsedUs()) / 1000.0 << " ms\n";
        std::cout << "Throughput: " << (static_cast<double>(engine.processedCount()) * 1'000'000.0 / static_cast<double>(engine.elapsedUs())) << " events/sec\n";
        std::cout << "Strategy PnL: " << static_cast<double>(strategy.getRealizedPnl()) / 10'000'000.0 << " USD\n";
        
    } catch (const std::exception& e) {
        std::cerr << "ERROR: " << e.what() << "\n";
        return 1;
    }
    
    return 0;
}