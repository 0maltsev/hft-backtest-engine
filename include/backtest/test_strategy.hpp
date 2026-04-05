#pragma once
#include "backtest/strategy.hpp"
#include "execution_engine.hpp"

#include <algorithm>
#include <cstdint>
#include <deque>
#include <iostream>

namespace backtest {

class TestStrategy : public BasicStrategy {
public:
    bool verbose = false;

    int64_t initial_capital_ticks = 100'000'000'000;
    int64_t commission_bps        = 10;
    int32_t order_quantity        = 100;

    int64_t window_us             = 500'000;
    double  imbalance_threshold   = 0.65;

    // --- выход ---
    int64_t take_profit_bps       = 15;
    int64_t stop_loss_bps         = 8;

    // --- ордера ---
    static constexpr int64_t ORDER_TIMEOUT_US       = 500'000;   // 0.5 сек
    static constexpr int64_t LIMIT_PRICE_BUFFER_BPS = 5;

    struct State {
        int64_t cash             = 0;
        int64_t position         = 0;
        int64_t entry_price      = 0;
        int64_t entry_commission = 0;
        int64_t realized_pnl     = 0;
        int64_t max_drawdown     = 0;
        int64_t peak_equity      = 0;
        int64_t total_commission = 0;
        size_t  trades_count     = 0;
        size_t  winning_trades   = 0;
        size_t  buy_signals      = 0;
        size_t  sell_signals     = 0;
    };

    void on_init() noexcept {
        state_           = State{};
        state_.cash      = initial_capital_ticks;
        state_.peak_equity = initial_capital_ticks;
        pending_orders_.clear();
        ofi_window_.clear();
        next_order_id_   = 1;

        execution_engine_.setCommissionBps(commission_bps);
    }

    void on_event(const MarketEvent& event) noexcept {
        updateOFI(event);
        checkPendingOrders(event);

        if (state_.position == 0) {
            tryEnter(event);
        } else {
            tryExit(event);
        }

        updateDrawdown(event);
    }

    void on_finish() noexcept {
        double win_rate = state_.trades_count > 0
            ? 100.0 * static_cast<double>(state_.winning_trades) / static_cast<double>(state_.trades_count) : 0.0;

        std::cout << "\n=== Strategy Results ===\n";
        std::cout << "Trades:           " << state_.trades_count << "\n";
        std::cout << "Win rate:         " << win_rate << " %\n";
        std::cout << "Buy signals:      " << state_.buy_signals << "\n";
        std::cout << "PnL (USD):        " << usd(state_.realized_pnl) << "\n";
        std::cout << "Commission (USD): " << usd(state_.total_commission) << "\n";
        std::cout << "Net PnL (USD):    " << usd(state_.realized_pnl) << "\n";
        std::cout << "Max Drawdown:     " << usd(state_.max_drawdown) << "\n";
        std::cout << "Final Cash (USD): " << usd(state_.cash) << "\n";
        std::cout << "========================\n";
    }

    [[nodiscard]] const State& getState() const noexcept { return state_; }
    [[nodiscard]] int64_t getRealizedPnl() const noexcept { return state_.realized_pnl; }
    [[nodiscard]] int64_t getMaxDrawdown()  const noexcept { return state_.max_drawdown; }
    [[nodiscard]] size_t  getTradesCount()  const noexcept { return state_.trades_count; }

private:

    struct OFITick {
        int64_t timestamp_us;
        int64_t buy_volume;
        int64_t sell_volume;
    };

    void updateOFI(const MarketEvent& event) noexcept {
        while (!ofi_window_.empty() &&
               event.timestamp_us - ofi_window_.front().timestamp_us > window_us) {
            ofi_window_.pop_front();
        }

        int64_t bv = (event.side == Side::Buy)  ? event.amount : 0;
        int64_t sv = (event.side == Side::Sell) ? event.amount : 0;
        ofi_window_.push_back({event.timestamp_us, bv, sv});
    }

    [[nodiscard]] double getImbalance() const noexcept {
        int64_t total_buy  = 0;
        int64_t total_sell = 0;
        for (const auto& t : ofi_window_) {
            total_buy  += t.buy_volume;
            total_sell += t.sell_volume;
        }
        int64_t total = total_buy + total_sell;
        if (total == 0) return 0.0;
        return static_cast<double>(total_buy - total_sell) / static_cast<double>(total);
    }

    void tryEnter(const MarketEvent& event) noexcept {
        if (!pending_orders_.empty()) return;

        double imb = getImbalance();

        if (imb >= imbalance_threshold) {
            ++state_.buy_signals;
            int64_t limit_price = event.price_ticks * (10'000 + LIMIT_PRICE_BUFFER_BPS) / 10'000;
            auto order = Order::limit_buy(limit_price, order_quantity,
                                          next_order_id_++, event.timestamp_us);
            pending_orders_.push_back(order);
        }
    }

    void tryExit(const MarketEvent& event) noexcept {
        if (state_.entry_price == 0) return;

        int64_t change_bps = ((event.price_ticks - state_.entry_price) * 10'000)
                              / state_.entry_price;

        bool take_profit = change_bps >=  take_profit_bps;
        bool stop_loss   = change_bps <= -stop_loss_bps;

        if (take_profit || stop_loss) {
            auto order = Order::market_sell(
                static_cast<int32_t>(state_.position),
                next_order_id_++,
                event.timestamp_us
            );
            auto report = execution_engine_.executeMarketOrder(order, event);
            if (report.status == OrderStatus::Filled) {
                applyExit(report, event.price_ticks);
            }
        }
    }

    void checkPendingOrders(const MarketEvent& event) noexcept {
        for (auto& order : pending_orders_) {
            if (!order.isActive()) continue;

            if (state_.position > 0) {
                order.status = OrderStatus::Cancelled;
                continue;
            }

            if (event.timestamp_us - order.timestamp_us > ORDER_TIMEOUT_US) {
                order.status = OrderStatus::Cancelled;
                continue;
            }

            auto report = execution_engine_.checkLimitOrder(order, event);
            if (report.status == OrderStatus::Filled) {
                order.status   = OrderStatus::Filled;
                order.filled_qty = report.filled_qty;
                applyEntry(report);
            }
        }

        pending_orders_.erase(
            std::remove_if(pending_orders_.begin(), pending_orders_.end(),
                [](const Order& o) { return !o.isActive(); }),
            pending_orders_.end()
        );
    }

    void applyEntry(const ExecutionReport& report) noexcept {
        state_.cash            -= report.filled_qty * report.avg_price + report.commission;
        state_.position        += report.filled_qty;
        state_.entry_price      = report.avg_price;
        state_.entry_commission = report.commission;
        state_.total_commission += report.commission;
        ++state_.trades_count;

        if (verbose) {
            std::cout << "[BUY]  price=" << report.avg_price
                      << " qty=" << report.filled_qty << "\n";
        }
    }

    void applyExit(const ExecutionReport& report, int64_t /*market_price*/) noexcept {
        int64_t gross_pnl = report.filled_qty * (report.avg_price - state_.entry_price);
        int64_t net_pnl   = gross_pnl - report.commission - state_.entry_commission;

        state_.cash            += report.filled_qty * report.avg_price - report.commission;
        state_.realized_pnl    += net_pnl;
        state_.total_commission += report.commission;

        if (net_pnl > 0) ++state_.winning_trades;

        if (verbose) {
            std::cout << "[SELL] price=" << report.avg_price
                      << " pnl=" << usd(net_pnl) << " USD\n";
        }

        state_.position        = 0;
        state_.entry_price     = 0;
        state_.entry_commission = 0;
        ++state_.trades_count;
    }

    void updateDrawdown(const MarketEvent& event) noexcept {
        int64_t equity = state_.cash + state_.position * event.price_ticks;
        if (equity > state_.peak_equity) {
            state_.peak_equity = equity;
        } else {
            int64_t dd = state_.peak_equity - equity;
            if (dd > state_.max_drawdown) state_.max_drawdown = dd;
        }
    }

    static double usd(int64_t ticks) noexcept {
        return static_cast<double>(ticks) / 10'000'000.0;
    }

    State                state_;
    std::deque<OFITick>  ofi_window_;
    std::vector<Order>   pending_orders_;
    int64_t              next_order_id_ = 1;
    ExecutionEngine      execution_engine_;
};

}