// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/content_settings/ios_cookie_blocker.h"

#import "ios/chrome/browser/browser_state/chrome_browser_state.h"
#import "ios/chrome/browser/main/browser.h"
#include "ios/chrome/browser/main/browser_list.h"
#include "ios/chrome/browser/main/browser_list_factory.h"
#include "ios/chrome/browser/web_state_list/web_state_list.h"
#import "ios/chrome/browser/web_state_list/web_usage_enabler/web_usage_enabler_browser_agent.h"
#import "ios/web/public/browsing_data/cookie_blocking_mode.h"
#import "ios/web/public/navigation/navigation_manager.h"
#import "ios/web/public/web_state.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

IOSCookieBlocker::IOSCookieBlocker(
    ChromeBrowserState* browser_state,
    scoped_refptr<content_settings::CookieSettings> cookie_settings)
    : browser_state_(browser_state), cookie_settings_(cookie_settings) {
  cookie_settings_->AddObserver(this);
  UpdateBrowserStateCookieBlockingMode();
}

void IOSCookieBlocker::Shutdown() {
  cookie_settings_->RemoveObserver(this);
  cookie_settings_.reset();
}

IOSCookieBlocker::~IOSCookieBlocker() = default;

web::CookieBlockingMode IOSCookieBlocker::GetCurrentCookieBlockingMode() {
  if (cookie_settings_->GetDefaultCookieSetting(/*provider_id=*/nullptr) ==
      CONTENT_SETTING_BLOCK) {
    return web::CookieBlockingMode::kBlock;
  }
  if (cookie_settings_->ShouldBlockThirdPartyCookies()) {
    return web::CookieBlockingMode::kBlockThirdParty;
  }
  return web::CookieBlockingMode::kAllow;
}

void IOSCookieBlocker::UpdateBrowserStateCookieBlockingMode() {
  web::CookieBlockingMode blocking_mode = GetCurrentCookieBlockingMode();
  if (blocking_mode != browser_state_->GetCookieBlockingMode()) {
    // This assumes that call to |SetCookieBlockingMode| will almost always be
    // synchronous. This is true because cookie blocking only has 3 modes, no
    // exceptions, so any async setup work can be done once for each mode at
    // initialization time. If cookie blocking ever changes to require async
    // work done here, when the blocking mode is set (e.g. compiling a new
    // ContentRuleList based on per-site exceptions), this should be re-examined
    // to see if something should be done to alert the user when the update
    // is finished.
    browser_state_->SetCookieBlockingMode(blocking_mode, base::DoNothing());
  }
}

void IOSCookieBlocker::OnThirdPartyCookieBlockingChanged(
    bool block_third_party_cookies) {
  UpdateBrowserStateCookieBlockingMode();
}

void IOSCookieBlocker::OnCookieSettingChanged() {
  UpdateBrowserStateCookieBlockingMode();
}
