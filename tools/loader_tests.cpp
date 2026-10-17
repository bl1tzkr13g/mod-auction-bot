#include <iostream>

void Addmod_auction_botScripts();
void Addmod_auctionsimScripts();

namespace
{
    unsigned scripts = 0;
    unsigned commands = 0;
    unsigned bridge = 0;
}

void AddAuctionSimScripts() { ++scripts; }
void AddSC_AuctionCommandScript() { ++commands; }
void AddAuctionSimAddonBridgeScript() { ++bridge; }

int main()
{
    Addmod_auction_botScripts();
    if (scripts != 1 || commands != 1 || bridge != 1)
    {
        std::cerr << "Fork loader did not register every component once\n";
        return 1;
    }
    scripts = commands = bridge = 0;
    Addmod_auctionsimScripts();
    if (scripts != 1 || commands != 1 || bridge != 1)
    {
        std::cerr << "Upstream loader compatibility failed\n";
        return 1;
    }
    std::cout << "Both module loader names linked and registered all components\n";
}
