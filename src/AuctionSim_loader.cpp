void AddAuctionSimScripts();
void AddSC_AuctionCommandScript();
void AddAuctionSimAddonBridgeScript();

// AzerothCore derives this entry point from the checkout directory name,
// replacing '-' with '_': modules/mod-auction-bot -> Addmod_auction_botScripts.
void Addmod_auction_botScripts()
{
    AddAuctionSimScripts();
    AddSC_AuctionCommandScript();
    AddAuctionSimAddonBridgeScript();
}

// Keep upstream installations under modules/mod-auctionsim working too.
void Addmod_auctionsimScripts()
{
    Addmod_auction_botScripts();
}
