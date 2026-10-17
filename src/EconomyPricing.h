#pragma once
#include <cstdint>
#include <string>
#include <unordered_map>

// Server-independent economy rules; all prices are copper per individual item.
namespace EconomyPricing
{
    constexpr uint32_t MaxPrice = 0x7FFFFFFE;
    enum class Mode { Historical, Custom, Hybrid };
    enum class ZeroVendor { Skip, Historical };
    struct Item
    {
        uint32_t id, itemClass, subclass, vendorSell;
    };
    struct Rules
    {
        double multiplier = 3.0;
        std::unordered_map<uint32_t, double> classes;
        std::unordered_map<uint32_t, double> subclasses; // class * 256 + subclass
        std::unordered_map<uint32_t, double> items;
        std::unordered_map<uint32_t, uint32_t> fixed;
        ZeroVendor zeroVendor = ZeroVendor::Skip;
        uint32_t Price(Item const& item, uint32_t historical) const;
        char const* Source(Item const& item) const;
    };
    struct Settings
    {
        Mode buyMode = Mode::Custom;
        Mode sellMode = Mode::Custom;
        Rules buy, sell;
        double variationPercent = 0.0;
        double minimumVendorMultiplier = 1.0;
        uint32_t dailyItemLimit = 0;
        uint64_t dailyCopperBudget = 0;
        bool applyToBids = true;
        uint32_t BuyCeiling(Item const& item, uint32_t historical, bool bid = false) const;
        uint32_t SellPrice(Item const& item, uint32_t historical, double variation = 0.0) const;
    };
    bool ParseMode(std::string text, Mode& out, bool allowHybrid = true);
    char const* ModeName(Mode mode);
    bool ParseMultipliers(std::string const& text, std::unordered_map<uint32_t, double>& out);
    bool ParseFixedPrices(std::string const& text, std::unordered_map<uint32_t, uint32_t>& out);
    // Per-item conf rows fill gaps in FixedPrices; the compact list takes priority.
    // DEFAULT leaves the individual row unset, so multiplier rules can apply.
    bool AddFixedPriceSetting(std::string const& itemId, std::string const& value, Rules& rules);
    bool ParseSubclasses(std::string const& text, std::unordered_map<uint32_t, double>& out);
    bool WithinStackLimit(uint32_t amount, uint32_t unitLimit, uint32_t count);
    uint32_t StackPrice(uint32_t unit, uint32_t count); // 0 when the total exceeds the core money limit

    // Conservative daily reservations: bids reserve their full commitment, even
    // when later outbid. Repeated bids on one auction only charge the increase.
    // Shared across houses; reset at UTC midnight. Owned by AuctionSim so service
    // reloads cannot reset it. Process restarts reset the ledger.
    class DailyLedger
    {
    public:
        bool CanCommit(int64_t now, uint32_t auction, uint32_t item, uint32_t count, uint32_t amount,
            uint32_t quantityLimit, uint64_t budget);
        void Commit(uint32_t auction, uint32_t item, uint32_t count, uint32_t amount);
    private:
        int64_t _day = -1;
        uint64_t _spent = 0;
        std::unordered_map<uint32_t, uint64_t> _quantities;
        std::unordered_map<uint32_t, uint32_t> _commitments;
    };
}
