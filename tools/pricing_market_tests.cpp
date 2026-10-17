#include "MarketEngine.h"
#include <cstdlib>
#include <iostream>

// Exercise the real buyer engine with an unchanged historical reference and a
// custom reference. A single deterministic buyer tier pays 0.25..0.5 x reference.
int main()
{
    Market::Faction faction;
    Market::Item item;
    item.itemId = 6522;
    item.ref = 10;
    item.buyersH = 100;
    item.curve = 0;
    faction.items.push_back(item);
    faction.demandScale = 1;
    Market::Curve curve;
    curve.fill(1.0f);
    faction.curves.push_back(curve);
    Market::Engine engine;
    Market::Listing listing;
    listing.itemIdx = 0;
    listing.perUnit = 20;
    listing.count = 1;
    listing.flags = Market::Listing::kBuyable;
    engine.Listings().push_back(listing);
    engine.BuildState(1);
    std::vector<uint32> claims;
    Market::Rng historicalRng(42);
    engine.PlanBuys(faction, 1, 0, historicalRng, claims);
    if (!claims.empty()) { std::cerr << "historical willingness exceeded\n"; return 1; }
    std::vector<uint32> references{100};
    Market::Rng customRng(42);
    engine.PlanBuys(faction, 1, 0, customRng, claims, &references);
    if (claims.size() != 1) { std::cerr << "custom willingness was not applied\n"; return 1; }
    if (faction.items[0].ref != 10) { std::cerr << "historical data mutated\n"; return 1; }
    claims.clear();
    references[0] = 0;
    Market::Rng zeroRng(42);
    engine.PlanBuys(faction, 1, 0, zeroRng, claims, &references);
    if (!claims.empty()) { std::cerr << "zero custom price bought an item\n"; return 1; }
    std::cout << "4 Market pricing checks passed\n";
}
