Debugging the legacy on-demand Windows client in Chrome requires a
patch to get rid of some DCHECKs in the code. Developers can apply the
patch `google_update_win.cc.patch` in a local branch and rebuild
Chrome with it. Don't land the patch with your changes!