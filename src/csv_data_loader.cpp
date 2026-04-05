#include "backtest/csv_data_loader.hpp"

#include <string_view>
#include <fstream>
#include <charconv>
#include <system_error>
#include <iostream>
#include <stdexcept>
#include <cctype>
#include <cstdlib>

namespace backtest {

CsvDataLoader::CsvDataLoader(const std::string& filepath)
    : filepath_(filepath) {}

const std::vector<MarketEvent>& CsvDataLoader::load() {
    if (loaded_) {
        return events_;
    }

    std::ifstream file(filepath_, std::ios::binary | std::ios::ate);
    if (!file.is_open()) {
        throw std::runtime_error(std::string("Failed to open file: ") + filepath_);
    }

    const std::streamsize file_size = file.tellg();
    file.seekg(0, std::ios::beg);

    // Средняя длина строки в trades.csv ~45-50 символов. 
    events_.reserve(static_cast<size_t>(file_size / 45) + 1);

    std::string line;
    bool header_checked = false;

    //TODO На этапах оптимизации заменим на fread() + кольцевой буфер.
    while (std::getline(file, line)) {
        if (line.empty()) continue;

        // Пропуск заголовка: если первая строка не начинается с цифры, считаем её заголовком
        if (!header_checked) {
            if (!std::isdigit(static_cast<unsigned char>(line.front()))) {
                header_checked = true;
                continue;
            }
            header_checked = true;
        }

        // Удаление \r для совместимости с Windows-файлами
        if (!line.empty() && line.back() == '\r') {
            line.pop_back();
        }

        MarketEvent event{};
        if (parseLine(std::string_view(line), event)) {
            events_.push_back(event);
        } else {
            std::cerr << "[WARN] Skipping malformed line: " << line << '\n';
        }
    }

    loaded_ = true;
    return events_;
}

// TODO  Оптимизируем позже через mmap + SIMD или binary+reinterpret_cast.
bool CsvDataLoader::parseLine(std::string_view line, MarketEvent& out) {
    size_t pos = 0;
    size_t comma_pos;

    //Индекс
    comma_pos = line.find(',', pos);
    if (comma_pos == std::string_view::npos) return false;
    pos = comma_pos + 1;

    //timestamp_us
    comma_pos = line.find(',', pos);
    if (comma_pos == std::string_view::npos) return false;
    std::string_view ts_view = line.substr(pos, comma_pos - pos);
    
    auto [ptr_ts, ec_ts] = std::from_chars(ts_view.data(), ts_view.data() + ts_view.size(), out.timestamp_us);
    if (ec_ts != std::errc{}) return false;
    pos = comma_pos + 1;

    //side
    comma_pos = line.find(',', pos);
    if (comma_pos == std::string_view::npos) return false;
    std::string_view side_view = line.substr(pos, comma_pos - pos);
    out.side = parseSide(side_view);
    out.type = EventType::Trade;
    pos = comma_pos + 1;

    //price
    comma_pos = line.find(',', pos);
    if (comma_pos == std::string_view::npos) return false;
    std::string_view price_view = line.substr(pos, comma_pos - pos);
    
    char* end_ptr = nullptr;
    double price_d = std::strtod(price_view.data(), &end_ptr);
    if (end_ptr == price_view.data()) return false;
    out.price_ticks = priceToTicks(price_d);
    pos = comma_pos + 1;

    //amount
    std::string_view amt_view = line.substr(pos);
    auto [ptr_amt, ec_amt] = std::from_chars(amt_view.data(), amt_view.data() + amt_view.size(), out.amount);
    if (ec_amt != std::errc{}) return false;

    return true;
}

int64_t CsvDataLoader::priceToTicks(double price) noexcept {
    // 7 знаков после запятой. +0.5 для корректного округления.
    return static_cast<int64_t>(price * 10'000'000.0 + 0.5);
}

Side CsvDataLoader::parseSide(std::string_view token) noexcept {
    if (token.empty()) return Side::None;
    const auto c = static_cast<char>(std::tolower(static_cast<unsigned char>(token[0])));
    if (c == 'b') return Side::Buy;
    if (c == 's') return Side::Sell;
    return Side::None;
}

size_t CsvDataLoader::eventCount() const noexcept { return events_.size(); }
int64_t CsvDataLoader::startTime() const noexcept { 
    return events_.empty() ? 0 : events_.front().timestamp_us; 
}
int64_t CsvDataLoader::endTime() const noexcept { 
    return events_.empty() ? 0 : events_.back().timestamp_us; 
}

}