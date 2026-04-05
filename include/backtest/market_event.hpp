#include <cstdint>

namespace backtest {
enum Side : uint8_t
{
    None = 0, 
    Buy = 1, 
    Sell = 2 
};

enum EventType : uint8_t
{
    Trade,
};

struct alignas(16) MarketEvent
{
    int64_t timestamp_us;
    EventType type;
    int64_t price_ticks;    // цена × множитель тика (например, 150.25 → 15025 при тике 0.01)
    int32_t amount;
    Side    side;
    uint8_t pad_[10]; // NOLINT
};
}