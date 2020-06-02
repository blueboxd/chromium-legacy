// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_PUBLIC_PROVIDER_CHROME_BROWSER_DISCOVER_FEED_DISCOVER_FEED_PROVIDER_H_
#define IOS_PUBLIC_PROVIDER_CHROME_BROWSER_DISCOVER_FEED_DISCOVER_FEED_PROVIDER_H_

#import <UIKit/UIKit.h>

#include "base/ios/block_types.h"

@protocol ApplicationCommands;

// DiscoverFeedProvider allows embedders to provide functionality for a Discover
// Feed.
class DiscoverFeedProvider {
 public:
  DiscoverFeedProvider() = default;
  virtual ~DiscoverFeedProvider() = default;

  DiscoverFeedProvider(const DiscoverFeedProvider&) = delete;
  DiscoverFeedProvider& operator=(const DiscoverFeedProvider&) = delete;

  // Returns true if the Discover Feed is enabled.
  virtual bool IsDiscoverFeedEnabled();
  // Returns the Discover Feed ViewController.
  virtual UIViewController* NewFeedViewController(
      id<ApplicationCommands> dispatcher) NS_RETURNS_RETAINED;
  // Refreshes the Discover Feed with completion.
  virtual void RefreshFeedWithCompletion(ProceduralBlock completion);
};

#endif  // IOS_PUBLIC_PROVIDER_CHROME_BROWSER_DISCOVER_FEED_DISCOVER_FEED_PROVIDER_H_
