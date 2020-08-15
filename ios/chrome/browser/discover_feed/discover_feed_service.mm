// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ios/chrome/browser/discover_feed/discover_feed_service.h"

#import "ios/public/provider/chrome/browser/discover_feed/discover_feed_provider.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

DiscoverFeedService::DiscoverFeedService(
    signin::IdentityManager* identity_manager,
    AuthenticationService* authentication_service,
    DiscoverFeedProvider* feed_provider)
    : identity_manager_(identity_manager),
      authentication_service_(authentication_service),
      discover_feed_provider_(feed_provider) {
  if (identity_manager_) {
    identity_manager_->AddObserver(this);
  }
  discover_feed_provider_->StartFeed(authentication_service_);
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
