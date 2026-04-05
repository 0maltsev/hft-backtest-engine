#pragma once
#include "backtest/strategy.hpp"
#include "execution_engine.hpp"

#include <algorithm>
#include <cstdint>
#include <iostream>
#include <vector>

namespace backtest {

class TestStrategy : public BasicStrategy {
public:
    bool verbose = false;
    int64_t initial_capital_ticks = 100'000'000'000;
    int64_t commission_bps = 10;
    int32_t order_quantity = 100;
    int64_t take_profit_bps = 20;

    static constexpr int64_t LIMIT_PRICE_BUFFER_BPS = 10;
    static constexpr int64_t ORDER_TIMEOUT_US = 10'000'000;

    struct State {
        int64_t cash = 0;
        int64_t position = 0;
        int64_t entry_price = 0;
        int64_t entry_commission = 0;
        size_t trades_count = 0;
        size_t buy_events = 0;
        size_t sell_events = 0;
        int64_t realized_pnl = 0;
        int64_t max_drawdown = 0;
        int64_t peak_equity = 0;
        int64_t total_commission = 0;
    };

    void on_init() noexcept {
        state_ = State{};
        state_.cash = initial_capital_ticks;
        pending_orders_.clear();
        next_order_id_ = 1;

        execution_engine_.setCommissionBps(commission_bps);

        if (verbose) {
            std::cout << "[Strategy] Initialized | Capital: "
                      << static_cast<double>(state_.cash) / 10'000'000.0 << " USD\n";
        }
    }

    void on_event(const MarketEvent& event) noexcept {
        if (event.side == Side::Buy) {
            ++state_.buy_events;
        } else if (event.side == Side::Sell) {
            ++state_.sell_events;
        }

        checkPendingOrders(event);

        if (state_.position == 0) {
            generateBuySignal(event);
        } else {
            checkTakeProfit(event);
        }

        updateDrawdown(event);
    }

    void on_finish() noexcept {
        std::cout << "\n=== Strategy Results ===\n";
        std::cout << "Events: " << (state_.buy_events + state_.sell_events) << "\n";
        std::cout << "Buy/Sell: " << state_.buy_events << " / " << state_.sell_events << "\n";
        std::cout << "Trades: " << state_.trades_count << "\n";
        std::cout << "PnL (ticks): " << state_.realized_pnl << "\n";
        std::cout << "PnL (USD): " << static_cast<double>(state_.realized_pnl) / 10'000'000.0 << "\n";
        std::cout << "Commission (USD): " << static_cast<double>(state_.total_commission) / 10'000'000.0 << "\n";
        std::cout << "Max Drawdown (USD): " << static_cast<double>(state_.max_drawdown) / 10'000'000.0 << "\n";
        std::cout << "Final Cash (USD): " << static_cast<double>(state_.cash) / 10'000'000.0 << "\n";
        std::cout << "======================\n";
    }

    [[nodiscard]] int64_t getRealizedPnl() const noexcept { return state_.realized_pnl; }
    [[nodiscard]] int64_t getMaxDrawdown() const noexcept { return state_.max_drawdown; }
    [[nodiscard]] size_t getTradesCount() const noexcept { return state_.trades_count; }
    [[nodiscard]] const State& getState() const noexcept { return state_; }

private:
    void checkPendingOrders(const MarketEvent& event) noexcept {
        for (auto& order : pending_orders_) {
            if (!order.isActive()) continue;

            if (state_.position > 0) {
                order.status = OrderStatus::Cancelled;
                continue;
            }

            int64_t order_age_us = event.timestamp_us - order.timestamp_us;
            if (order_age_us > ORDER_TIMEOUT_US) {
                order.status = OrderStatus::Cancelled;
                continue;
            }

            ExecutionReport report = execution_engine_.checkLimitOrder(order, event);

            if (report.status == OrderStatus::Filled) {
                order.status = OrderStatus::Filled;
                order.filled_qty = report.filled_qty;
                applyExecution(report, order.side);
            }
        }

        pending_orders_.erase(
            std::remove_if(pending_orders_.begin(), pending_orders_.end(),
                [](const Order& o) { return !o.isActive(); }),
            pending_orders_.end()
        );
    }

    void generateBuySignal(const MarketEvent& event) noexcept {
        if (!pending_orders_.empty()) return;

        if (event.side == Side::Sell) {
            int64_t limit_price = event.price_ticks * (10'000 + LIMIT_PRICE_BUFFER_BPS) / 10'000;

            Order order = Order::limit_buy(
                limit_price,
                order_quantity,
                next_order_id_++,
                event.timestamp_us
            );
            pending_orders_.push_back(order);

            if (verbose) {
                std::cout << "[Strategy] Placed buy order #" << order.order_id
                          << " @ " << order.price_ticks << "\n";
            }
        }
    }

    void checkTakeProfit(const MarketEvent& event) noexcept {
        if (state_.entry_price == 0) return;

        int64_t price_change_bps = ((event.price_ticks - state_.entry_price) * 10'000)
                                    / state_.entry_price;

        if (price_change_bps >= take_profit_bps) {
            Order order = Order::market_sell(
                static_cast<int32_t>(state_.position),
                next_order_id_++,
                event.timestamp_us
            );

            ExecutionReport report = execution_engine_.executeMarketOrder(order, event);

            if (report.status == OrderStatus::Filled) {
                applyExecution(report, OrderSide::Sell);
            }
        }
    }

    void applyExecution(const ExecutionReport& report, OrderSide side) noexcept {
        if (side == OrderSide::Buy) {
            state_.cash -= report.filled_qty * report.avg_price + report.commission;
            state_.position += report.filled_qty;
            state_.entry_price = report.avg_price;
            state_.entry_commission = report.commission;
        } else {
            state_.cash += report.filled_qty * report.avg_price - report.commission;
            state_.realized_pnl += report.filled_qty * (report.avg_price - state_.entry_price)
                                  - report.commission
                                  - state_.entry_commission;

            state_.position = 0;
            state_.entry_price = 0;
            state_.entry_commission = 0;
        }

        state_.total_commission += report.commission;
        ++state_.trades_count;

        if (verbose && state_.trades_count % 100 == 0) {
            std::cout << "[Strategy] Trade #" << state_.trades_count
                      << " | Side: " << (side == OrderSide::Buy ? "BUY" : "SELL")
                      << " | PnL: " << static_cast<double>(state_.realized_pnl) / 10'000'000.0
                      << " USD | Commission: " << static_cast<double>(report.commission) / 10'000'000.0
                      << " USD\n";
        }
    }

    void updateDrawdown(const MarketEvent& event) noexcept {
        int64_t current_equity = state_.cash + (state_.position * event.price_ticks);

        if (current_equity > state_.peak_equity) {
            state_.peak_equity = current_equity;
        } else {
            int64_t drawdown = state_.peak_equity - current_equity;
            if (drawdown > state_.max_drawdown) {
                state_.max_drawdown = drawdown;
            }
        }
    }

    State state_;
    std::vector<Order> pending_orders_;
    int64_t next_order_id_ = 1;
    ExecutionEngine execution_engine_;
};

}