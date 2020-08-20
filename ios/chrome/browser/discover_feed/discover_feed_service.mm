// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ios/chrome/browser/discover_feed/discover_feed_service.h"

#include "ios/chrome/browser/browser_state/chrome_browser_state.h"
#import "ios/chrome/browser/signin/authentication_service_factory.h"
#include "ios/chrome/browser/signin/identity_manager_factory.h"
#import "ios/public/provider/chrome/browser/chrome_browser_provider.h"
#import "ios/public/provider/chrome/browser/discover_feed/discover_feed_configuration.h"
#import "ios/public/provider/chrome/browser/discover_feed/discover_feed_provider.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

DiscoverFeedService::DiscoverFeedService(ChromeBrowserState* browser_state) {
  discover_feed_provider_ =
      ios::GetChromeBrowserProvider()->GetDiscoverFeedProvider();
  identity_manager_ = IdentityManagerFactory::GetForBrowserState(browser_state);
  if (identity_manager_) {
    identity_manager_->AddObserver(this);
  }

  DiscoverFeedConfiguration* discover_config =
      [[DiscoverFeedConfiguration alloc] init];
  discover_config.browserState = browser_state;
  discover_feed_provider_->StartFeed(discover_config);
}

DiscoverFeedService::~DiscoverFeedService() {}

void DiscoverFeedService::Shutdown() {
  if (identity_manager_) {
    identity_manager_->RemoveObserver(this);
  }
}

void DiscoverFeedService::OnPrimaryAccountSet(
    const CoreAccountInfo& primary_account_info) {
  discover_feed_provider_->UpdateFeedForAccountChange();
}

void DiscoverFeedService::OnPrimaryAccountCleared(
    const CoreAccountInfo& previous_primary_account_info) {
  discover_feed_provider_->UpdateFeedForAccountChange();
}
