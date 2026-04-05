#pragma once

#include "backtest/strategy.hpp"
#include <algorithm>
#include <iostream>
#include <cstdint>

namespace backtest {

class TestStrategy : public BasicStrategy {
public:
    // ------------------------------------------------------------------------
    // Конфигурация (настраивается перед запуском)
    // ------------------------------------------------------------------------
    bool verbose = false;                    // Выводить лог каждого N-ного события
    int64_t initial_capital_ticks = 100'000'000'000;  // Начальный капитал (в тиках)
    
    // ------------------------------------------------------------------------
    // Внутреннее состояние
    // ------------------------------------------------------------------------
    struct State {
        int64_t cash {0};                    // Доступные средства (тики)
        int64_t position {0};                // Позиция (лоты, >0 = long)
        int64_t entry_price {0};             // Цена входа (тики)
        size_t trades_count {0};             // Всего сделок
        size_t buy_events {0};               // Событий на покупку
        size_t sell_events {0};              // Событий на продажу
        int64_t realized_pnl {0};            // Реализованный PnL (тики)
        int64_t max_drawdown {0};            // Максимальная просадка (тики)
        int64_t peak_equity {0};             // Пик эквити для расчёта просадки
    };
    
    // ------------------------------------------------------------------------
    // on_init: вызывается 1 раз перед стартом
    // ------------------------------------------------------------------------
    void on_init() noexcept {
        state_ = State{};
        state_.cash = initial_capital_ticks;
        
        if (verbose) {
            std::cout << "[Strategy] Initialized | Capital: " 
                    << static_cast<double>(state_.cash) / 10'000'000.0 << " USD\n";
        }
    }
    
    // ------------------------------------------------------------------------
    // on_event: вызывается на каждый тик
    // ------------------------------------------------------------------------
    void on_event(const MarketEvent& event) noexcept {
        // 1. Сбор статистики (дешёвые операции)
        if (event.side == Side::Buy) {
            ++state_.buy_events;
        } else if (event.side == Side::Sell) {
            ++state_.sell_events;
        }
        
        constexpr int64_t take_profit_bps = 10;
        
        if (state_.position == 0 && state_.cash > 0) {
            if (event.price_ticks <= state_.cash) {
                state_.cash -= event.price_ticks;
                state_.position = 1;
                state_.entry_price = event.price_ticks;
                ++state_.trades_count;
            }
        } 
        else if (state_.position > 0) {
            int64_t price_change_bps = ((event.price_ticks - state_.entry_price) * 10'000) / state_.entry_price;
            
            if (price_change_bps >= take_profit_bps) {
                state_.cash += event.price_ticks;
                state_.realized_pnl += event.price_ticks - state_.entry_price;
                state_.position = 0;
                ++state_.trades_count;
            }
        }
        
        int64_t current_equity = state_.cash + (state_.position * event.price_ticks);
        if (current_equity > state_.peak_equity) {
            state_.peak_equity = current_equity;
        } else {
            int64_t drawdown = state_.peak_equity - current_equity;
            state_.max_drawdown = std::max(drawdown, state_.max_drawdown);
        }
        
        // 4. Отладочный вывод (только если включён и редко)
        if (verbose && state_.trades_count > 0 && state_.trades_count % 100'000 == 0) {
            std::cout << "[Strategy] Trade #" << state_.trades_count 
                    << " | PnL: " << static_cast<double>(state_.realized_pnl) / 10'000'000.0 << " USD\n";
        }
    }
    
    // ------------------------------------------------------------------------
    // on_finish: вызывается 1 раз после завершения
    // ------------------------------------------------------------------------
    void on_finish() const noexcept {
        std::cout << "\n=== Strategy Results ===\n";
        std::cout << "Events: " << (state_.buy_events + state_.sell_events) << "\n";
        std::cout << "Buy/Sell: " << state_.buy_events << " / " << state_.sell_events << "\n";
        std::cout << "Trades: " << state_.trades_count << "\n";
        std::cout << "PnL (ticks): " << state_.realized_pnl << "\n";
        std::cout << "PnL (USD): " << static_cast<double>(state_.realized_pnl) / 10'000'000.0 << "\n";
        std::cout << "Max Drawdown (USD): " << static_cast<double>(state_.max_drawdown) / 10'000'000.0 << "\n";
        std::cout << "Final Cash (USD): " << static_cast<double>(state_.cash) / 10'000'000.0 << "\n";
        std::cout << "======================\n";
    }
    
    // ------------------------------------------------------------------------
    // Геттеры для внешней аналитики
    // ------------------------------------------------------------------------
    [[nodiscard]] int64_t getRealizedPnl() const noexcept { return state_.realized_pnl; }
    [[nodiscard]] int64_t getMaxDrawdown() const noexcept { return state_.max_drawdown; }
    [[nodiscard]] size_t getTradesCount() const noexcept { return state_.trades_count; }
    [[nodiscard]] const State& getState() const noexcept { return state_; }
    
private:
    State state_;
};

}