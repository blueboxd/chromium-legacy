// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_DISCOVER_FEED_DISCOVER_FEED_SERVICE_H_
#define IOS_CHROME_BROWSER_DISCOVER_FEED_DISCOVER_FEED_SERVICE_H_

#include "components/keyed_service/core/keyed_service.h"
#include "components/signin/public/identity_manager/identity_manager.h"

class ChromeBrowserState;
class DiscoverFeedProvider;

// A browser-context keyed service that is used to keep the Discover Feed data
// up to date.
class DiscoverFeedService : public KeyedService,
                            public signin::IdentityManager::Observer {
 public:
  // Initializes the service.
  DiscoverFeedService(ChromeBrowserState* browser_state);
  ~DiscoverFeedService() override;

  // KeyedService:
  void Shutdown() override;

 private:
  // IdentityManager::Observer.
  void OnPrimaryAccountSet(
      const CoreAccountInfo& primary_account_info) override;
  void OnPrimaryAccountCleared(
      const CoreAccountInfo& previous_primary_account_info) override;

  // Identity manager to observe.
  signin::IdentityManager* identity_manager_;

  // Discover Feed provider to notify of changes.
  DiscoverFeedProvider* discover_feed_provider_;

  DISALLOW_COPY_AND_ASSIGN(DiscoverFeedService);
};

#endif  // IOS_CHROME_BROWSER_DISCOVER_FEED_DISCOVER_FEED_SERVICE_H_
