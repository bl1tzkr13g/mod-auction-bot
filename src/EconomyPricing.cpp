#include "EconomyPricing.h"
#include <algorithm>
#include <cctype>
#include <cmath>
#include <sstream>
#include <vector>
#include <utility>
#include "ASParse.h"

namespace EconomyPricing
{
    namespace
    {
        uint32_t Scale(uint32_t value, double multiplier)
        {
            double result = std::floor(value * multiplier);
            if (!std::isfinite(result) || result > MaxPrice) return 0;
            return result > 0 ? static_cast<uint32_t>(result) : 0;
        }
        template<class Fn> bool ParseEntries(std::string text, Fn parse)
        {
            if (text.find_first_not_of(" \t\r\n") == std::string::npos) return true;
            size_t start = 0;
            for (size_t at = 0; at <= text.size(); ++at)
            {
                if (at == text.size() || text[at] == ',')
                {
                    if (text.substr(start, at - start).find_first_not_of(" \t\r\n") == std::string::npos)
                        return false;
                    start = at + 1;
                }
            }
            std::replace(text.begin(), text.end(), ',', ' ');
            std::istringstream input(text);
            std::string token;
            while (input >> token)
            {
                std::vector<std::string> fields;
                std::istringstream row(token);
                std::string field;
                while (std::getline(row, field, ':')) fields.push_back(field);
                if (token.back() == ':' || !parse(fields)) return false;
            }
            return true;
        }
    }
    uint32_t Rules::Price(Item const& item, uint32_t historical) const
    {
        if (auto it = fixed.find(item.id); it != fixed.end()) return it->second;
        if (item.vendorSell == 0) return zeroVendor == ZeroVendor::Historical ? historical : 0;
        double factor = multiplier;
        if (auto it = classes.find(item.itemClass); it != classes.end()) factor = it->second;
        if (auto it = subclasses.find(item.itemClass * 256 + item.subclass); it != subclasses.end()) factor = it->second;
        if (auto it = items.find(item.id); it != items.end()) factor = it->second;
        return Scale(item.vendorSell, factor);
    }
    char const* Rules::Source(Item const& item) const
    {
        if (fixed.count(item.id)) return "fixed item price";
        if (!item.vendorSell) return zeroVendor == ZeroVendor::Historical ? "historical fallback" : "zero vendor: skip";
        if (items.count(item.id)) return "item multiplier";
        if (subclasses.count(item.itemClass * 256 + item.subclass)) return "subclass multiplier";
        if (classes.count(item.itemClass)) return "class multiplier";
        return "default multiplier";
    }
    uint32_t Settings::BuyCeiling(Item const& item, uint32_t historical, bool bid) const
    {
        if (buyMode == Mode::Historical || (bid && !applyToBids)) return historical;
        uint32_t custom = buy.Price(item, historical);
        return buyMode == Mode::Hybrid ? std::min(custom, historical) : custom;
    }
    uint32_t Settings::SellPrice(Item const& item, uint32_t historical, double variation) const
    {
        if (sellMode == Mode::Historical) return historical;
        uint32_t price = sell.Price(item, historical);
        if (!price) return 0;
        price = Scale(price, 1.0 + std::clamp(variation, -1.0, 1.0) * variationPercent / 100.0);
        if (!price) return 0;
        if (item.vendorSell * minimumVendorMultiplier > MaxPrice) return 0;
        return std::max(price, Scale(item.vendorSell, minimumVendorMultiplier));
    }
    bool WithinStackLimit(uint32_t amount, uint32_t unitLimit, uint32_t count)
    {
        return amount && unitLimit && count && uint64_t(amount) <= uint64_t(unitLimit) * count;
    }
    uint32_t StackPrice(uint32_t unit, uint32_t count)
    {
        uint64_t total = uint64_t(unit) * count;
        return total <= MaxPrice ? static_cast<uint32_t>(total) : 0;
    }
    bool ParseMode(std::string text, Mode& out, bool allowHybrid)
    {
        std::transform(text.begin(), text.end(), text.begin(), [](unsigned char c) { return std::toupper(c); });
        if (text == "HISTORICAL") out = Mode::Historical;
        else if (text == "CUSTOM") out = Mode::Custom;
        else if (text == "HYBRID" && allowHybrid) out = Mode::Hybrid;
        else return false;
        return true;
    }
    char const* ModeName(Mode mode)
    {
        return mode == Mode::Historical ? "HISTORICAL" : mode == Mode::Custom ? "CUSTOM" : "HYBRID";
    }
    bool ParseMultipliers(std::string const& text, std::unordered_map<uint32_t, double>& out)
    {
        std::unordered_map<uint32_t, double> parsed;
        bool ok = ParseEntries(text, [&](auto const& fields) {
            uint32_t id; float value;
            if (fields.size() != 2 || !ASParse::Integer(fields[0], id) || !id ||
                !ASParse::Float(fields[1], value) || !std::isfinite(value) || value < 0) return false;
            return parsed.emplace(id, value).second;
        });
        if (ok) out = std::move(parsed);
        return ok;
    }
    bool ParseFixedPrices(std::string const& text, std::unordered_map<uint32_t, uint32_t>& out)
    {
        std::unordered_map<uint32_t, uint32_t> parsed;
        bool ok = ParseEntries(text, [&](auto const& fields) {
            uint32_t id, value;
            if (fields.size() != 2 || !ASParse::Integer(fields[0], id) || !id ||
                !ASParse::Integer(fields[1], value) || value > MaxPrice) return false;
            return parsed.emplace(id, value).second;
        });
        if (ok) out = std::move(parsed);
        return ok;
    }
    bool AddFixedPriceSetting(std::string const& itemId, std::string const& value, Rules& rules)
    {
        uint32_t id;
        std::unordered_map<uint32_t, uint32_t> parsed;
        if (!ASParse::Integer(itemId, id) || !id || std::to_string(id) != itemId)
            return false;
        if (value == "DEFAULT") return true; // Ignore this row; compact overrides still win.
        if (!ParseFixedPrices(itemId + ":" + value, parsed) || parsed.size() != 1)
            return false;
        rules.fixed.emplace(id, parsed.at(id));
        return true;
    }
    bool ParseSubclasses(std::string const& text, std::unordered_map<uint32_t, double>& out)
    {
        std::unordered_map<uint32_t, double> parsed;
        bool ok = ParseEntries(text, [&](auto const& fields) {
            uint32_t cls, sub; float value;
            if (fields.size() != 3 || !ASParse::Integer(fields[0], cls) || cls >= 17 ||
                !ASParse::Integer(fields[1], sub) || sub >= 256 || !ASParse::Float(fields[2], value) ||
                !std::isfinite(value) || value < 0) return false;
            return parsed.emplace(cls * 256 + sub, value).second;
        });
        if (ok) out = std::move(parsed);
        return ok;
    }
    bool DailyLedger::CanCommit(int64_t now, uint32_t auction, uint32_t item, uint32_t count, uint32_t amount,
        uint32_t quantityLimit, uint64_t budget)
    {
        if (_day != now / 86400)
        {
            _day = now / 86400; _spent = 0; _quantities.clear(); _commitments.clear();
        }
        auto previous = _commitments.find(auction);
        uint64_t quantity = previous == _commitments.end() ? count : 0;
        uint64_t cost = previous == _commitments.end() ? amount : amount > previous->second ? amount - previous->second : 0;
        auto found = _quantities.find(item);
        uint64_t used = found == _quantities.end() ? 0 : found->second;
        return (!quantityLimit || used + quantity <= quantityLimit) && (!budget || _spent + cost <= budget);
    }
    void DailyLedger::Commit(uint32_t auction, uint32_t item, uint32_t count, uint32_t amount)
    {
        auto [previous, inserted] = _commitments.emplace(auction, amount);
        if (inserted) { _quantities[item] += count; _spent += amount; }
        else if (amount > previous->second) { _spent += amount - previous->second; previous->second = amount; }
    }
}
