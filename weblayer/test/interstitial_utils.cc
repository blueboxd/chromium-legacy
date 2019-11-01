// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "weblayer/test/interstitial_utils.h"

#include "components/security_interstitials/content/security_interstitial_tab_helper.h"
#include "weblayer/browser/browser_controller_impl.h"
#include "weblayer/browser/ssl_blocking_page.h"

namespace weblayer {

namespace {

// Returns the security interstitial currently showing in |browser_controller|,
// or null if there is no such interstitial.
security_interstitials::SecurityInterstitialPage*
GetCurrentlyShowingInterstitial(BrowserController* browser_controller) {
  BrowserControllerImpl* browser_controller_impl =
      static_cast<BrowserControllerImpl*>(browser_controller);

  security_interstitials::SecurityInterstitialTabHelper* helper =
      security_interstitials::SecurityInterstitialTabHelper::FromWebContents(
          browser_controller_impl->web_contents());

  return helper
             ? helper
                   ->GetBlockingPageForCurrentlyCommittedNavigationForTesting()
             : nullptr;
}

}  // namespace

bool IsShowingSecurityInterstitial(BrowserController* browser_controller) {
  return GetCurrentlyShowingInterstitial(browser_controller) != nullptr;
}

bool IsShowingSSLInterstitial(BrowserController* browser_controller) {
  auto* blocking_page = GetCurrentlyShowingInterstitial(browser_controller);

  if (!blocking_page)
    return false;

  return blocking_page->GetTypeForTesting() == SSLBlockingPage::kTypeForTesting;
}

}  // namespace weblayer
