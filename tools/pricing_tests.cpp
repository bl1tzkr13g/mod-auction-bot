#include "EconomyPricing.h"
#include <cstdlib>
#include <fstream>
#include <iostream>
#include <limits>
#include <sstream>
using namespace EconomyPricing;
namespace
{
    int checks = 0;
    void Check(bool condition, char const* name)
    {
        ++checks;
        if (!condition) { std::cerr << "FAIL: " << name << '\n'; std::exit(1); }
    }
}
int main(int argc, char** argv)
{
    Item fish{6522, 7, 0, 1000};
    Settings s;
    Check(s.SellPrice(fish, 34000) == 3000, "3x vendor sell baseline");
    Check(StackPrice(s.SellPrice(fish, 34000), 20) == 60000, "20 item stack");
    Check(s.buyMode == Mode::Custom, "custom buying is the default");
    s.buy.multiplier = 5;
    Check(s.BuyCeiling(fish, 1000) == 5000, "configurable default buying multiplier");
    s.buy.fixed[6522] = 7000;
    Check(s.BuyCeiling(fish, 1000) == 7000, "fixed buying price overrides default");
    Check(s.BuyCeiling(Item{2447, 7, 0, 100}, 1) == 500, "unlisted item inherits default");
    s.buy.fixed.clear();
    s.buy.multiplier = 3;
    s.buyMode = Mode::Hybrid;
    Check(s.BuyCeiling(fish, 34000) == 3000, "hybrid custom cap");
    Check(s.BuyCeiling(fish, 2000) == 2000, "hybrid historical cap");
    s.buyMode = Mode::Custom;
    Check(s.BuyCeiling(fish, 2000) == 3000, "custom independent of history");
    s.sell.classes[7] = 4;
    Check(s.SellPrice(fish, 0) == 4000, "class rule");
    s.sell.subclasses[7 * 256] = 5;
    Check(s.SellPrice(fish, 0) == 5000, "subclass rule");
    s.sell.items[6522] = 6;
    Check(s.SellPrice(fish, 0) == 6000, "item multiplier rule");
    s.sell.fixed[6522] = 10000;
    Check(s.SellPrice(fish, 0) == 10000, "fixed overrides multiplier");
    Check(s.BuyCeiling(fish, 0) == 3000, "buy and sell independent");
    s.sell.fixed[6522] = 1;
    Check(s.SellPrice(fish, 0) == 1000, "vendor floor");
    s.sell.fixed[6522] = 0;
    Check(s.SellPrice(fish, 0) == 0, "fixed zero disables listing");
    Item noVendor{1, 0, 0, 0};
    Check(s.SellPrice(noVendor, 250) == 0, "zero vendor skips");
    s.sell.zeroVendor = ZeroVendor::Historical;
    Check(s.SellPrice(noVendor, 250) == 250, "zero vendor history fallback");
    s.sell.fixed[1] = 600;
    Check(s.SellPrice(noVendor, 250) == 600, "fixed price for zero vendor");
    s.sellMode = Mode::Historical;
    Check(s.SellPrice(fish, 123) == 123, "historical ignores custom rules");
    s.buyMode = Mode::Historical;
    Check(s.BuyCeiling(fish, 123) == 123, "historical buying");
    s.buyMode = Mode::Hybrid; s.applyToBids = false;
    Check(s.BuyCeiling(fish, 5000, true) == 5000, "bid price caps optional");
    Check(s.BuyCeiling(fish, 5000) == 3000, "buyout caps still active");
    s.sellMode = Mode::Custom; s.sell.fixed[6522] = 10000; s.variationPercent = 10;
    Check(s.SellPrice(fish, 0, -1) == 9000 && s.SellPrice(fish, 0, 1) == 11000, "variation band");
    Check(WithinStackLimit(60000, 3000, 20), "exact stack cap accepted");
    Check(!WithinStackLimit(60001, 3000, 20), "fractional unit excess rejected");
    Check(!WithinStackLimit(1, 0, 20) && !WithinStackLimit(1, 1, 0), "zero cap or count rejected");
    Check(std::string(s.sell.Source(fish)) == "fixed item price", "inspection source");
    s.minimumVendorMultiplier = 1e20;
    Check(s.SellPrice(fish, 0) == 0, "overflowing floor rejects sale");
    s.minimumVendorMultiplier = 1;
    Check(StackPrice(MaxPrice, 2) == 0, "stack overflow rejected");
    Check(StackPrice(MaxPrice, 1) == MaxPrice, "maximum supported price");
    s.sell.fixed.clear(); s.sell.items[6522] = std::numeric_limits<double>::infinity();
    Check(s.SellPrice(fish, 0) == 0, "nonfinite computed price rejected");
    Mode mode = Mode::Custom;
    Check(ParseMode("historical", mode) && mode == Mode::Historical, "case insensitive mode");
    Check(!ParseMode("hybrid", mode, false) && !ParseMode("invalid", mode), "invalid sell mode");
    std::unordered_map<uint32_t, double> multipliers;
    Check(ParseMultipliers("6522:20,2447:4.5", multipliers) && multipliers.at(6522) == 20, "multiple multipliers");
    Check(!ParseMultipliers("6522:nan", multipliers) && multipliers.size() == 2, "invalid parse atomic");
    Check(!ParseMultipliers("6522:-1", multipliers), "negative multiplier rejected");
    Check(!ParseMultipliers("6522:1,6522:2", multipliers), "duplicate rejected");
    Check(!ParseMultipliers("6522:inf", multipliers), "infinity rejected");
    Check(!ParseMultipliers("6522:1garbage", multipliers), "trailing garbage rejected");
    Check(!ParseMultipliers("6522:1,,2447:2", multipliers), "empty rule rejected");
    Check(!ParseMultipliers("6522:1,", multipliers), "trailing comma rejected");
    Check(!ParseMultipliers("0:1", multipliers), "zero multiplier item ID rejected");
    Check(!ParseMultipliers("6522:1:", multipliers), "extra separator rejected");
    Check(ParseMultipliers("", multipliers) && multipliers.empty(), "empty clears rules");
    Check(ParseSubclasses("7:0:5", multipliers) && multipliers.at(7 * 256) == 5, "subclass parser");
    Check(!ParseSubclasses("17:0:5", multipliers), "class bounds");
    Check(!ParseSubclasses("7:256:5", multipliers), "subclass bounds");
    std::unordered_map<uint32_t, uint32_t> prices;
    Check(ParseFixedPrices("6522:10000", prices) && prices.at(6522) == 10000, "copper fixed parser");
    Check(!ParseFixedPrices("6522:9999999999999", prices), "oversized fixed price rejected");
    Check(!ParseFixedPrices("6522:2.5", prices), "fractional copper rejected");
    Check(!ParseFixedPrices("0:1", prices), "zero item rejected");
    Rules named;
    Check(AddFixedPriceSetting("2589", "100", named) && named.fixed.at(2589) == 100,
        "individual config row");
    Check(AddFixedPriceSetting("2589", "150", named) && named.fixed.at(2589) == 100,
        "compact list keeps priority over individual rows");
    Check(AddFixedPriceSetting("2592", "0", named) && named.fixed.at(2592) == 0,
        "individual row can disable an item");
    Check(AddFixedPriceSetting("2447", "DEFAULT", named) && !named.fixed.count(2447),
        "DEFAULT falls through to multiplier rules");
    Check(AddFixedPriceSetting("2589", "DEFAULT", named) && named.fixed.at(2589) == 100,
        "DEFAULT does not erase an explicit compact override");
    Check(!AddFixedPriceSetting("02589", "100", named), "noncanonical item ID rejected");
    Check(!AddFixedPriceSetting("2589", "-100", named), "negative individual price rejected");
    Check(!AddFixedPriceSetting("2589", "100,2447:500", named), "extra row injection rejected");
    Check(!AddFixedPriceSetting("2589", "2147483647", named), "individual price money limit");

    // Exercise the shipped starter rows through the same parser the server uses.
    std::ifstream conf(argc > 1 ? argv[1] : "conf/auctionsim.conf.dist");
    Check(conf.is_open(), "distributed config available");
    Rules starterBuy, starterSell;
    std::string line;
    while (std::getline(conf, line))
    {
        for (auto side : {std::string("Buy"), std::string("Sell")})
        {
            std::string prefix = "AuctionSim.Pricing." + side + ".FixedPrice.";
            if (line.compare(0, prefix.size(), prefix) != 0) continue;
            size_t equals = line.find(" = ", prefix.size());
            Check(equals != std::string::npos, "starter row assignment");
            std::string id = line.substr(prefix.size(), equals - prefix.size());
            std::string value;
            std::istringstream(line.substr(equals + 3)) >> value;
            Rules& target = side == "Buy" ? starterBuy : starterSell;
            size_t before = target.fixed.size();
            Check(AddFixedPriceSetting(id, value, target), "starter row valid");
            Check(target.fixed.size() == before + 1, "no duplicate starter item");
        }
    }
    Check(starterBuy.fixed.size() >= 7 && starterBuy.fixed.size() == starterSell.fixed.size(),
        "starter prices exist on both sides");
    for (auto const& [id, buy] : starterBuy.fixed)
    {
        auto sell = starterSell.fixed.find(id);
        Check(sell != starterSell.fixed.end() && buy > 0 && buy < sell->second,
            "starter price has a positive trading margin");
    }
    for (uint32_t cloth : {2589u, 2592u, 4306u, 4338u, 14047u, 21877u, 33470u})
        Check(starterBuy.fixed.count(cloth) && starterSell.fixed.count(cloth), "all cloth tiers included");
    Check(starterBuy.Price(Item{10940, 7, 12, 0}, 0) == 100,
        "zero-vendor Strange Dust now has a custom buying price");
    starterBuy.multiplier = 999;
    Check(starterBuy.Price(Item{2589, 7, 5, 13}, 0) == 100, "starter fixed price overrides changed default");
    size_t starterCount = starterBuy.fixed.size();
    starterBuy.fixed.erase(2589);
    Check(starterBuy.Price(Item{2589, 7, 5, 13}, 0) == 13 * 999,
        "removing starter row restores configurable default");
    std::cout << starterCount << " starter items validated\n";
    DailyLedger ledger;
    Check(ledger.CanCommit(0, 1, 6522, 20, 60000, 40, 120000), "initial daily budget");
    ledger.Commit(1, 6522, 20, 60000);
    Check(ledger.CanCommit(0, 2, 6522, 20, 60000, 40, 120000), "exact remaining budget");
    ledger.Commit(2, 6522, 20, 60000);
    Check(!ledger.CanCommit(0, 3, 6522, 1, 1, 40, 0), "quantity not stack count");
    Check(!ledger.CanCommit(0, 3, 2447, 1, 1, 0, 120000), "shared gold budget");
    Check(ledger.CanCommit(0, 1, 6522, 20, 60000, 40, 120000), "same bid no double charge");
    Check(!ledger.CanCommit(0, 1, 6522, 20, 60001, 40, 120000), "bid increase budget");
    Check(ledger.CanCommit(86400, 3, 6522, 20, 60000, 20, 60000), "UTC day reset");
    DailyLedger bids;
    Check(bids.CanCommit(0, 1, 6522, 20, 100, 20, 200), "opening bid");
    bids.Commit(1, 6522, 20, 100);
    Check(bids.CanCommit(0, 1, 6522, 20, 200, 20, 200), "rebid incremental");
    bids.Commit(1, 6522, 20, 200);
    Check(!bids.CanCommit(0, 2, 2447, 1, 1, 0, 200), "rebid actually charged");
    std::cout << checks << " pricing checks passed\n";
}
