// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_PUBLIC_PROVIDER_CHROME_BROWSER_DISCOVER_FEED_DISCOVER_FEED_PROVIDER_H_
#define IOS_PUBLIC_PROVIDER_CHROME_BROWSER_DISCOVER_FEED_DISCOVER_FEED_PROVIDER_H_

#import <UIKit/UIKit.h>

#include "base/ios/block_types.h"

@protocol ApplicationCommands;
class Browser;

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
  // Deprecated - use the below NewFeedViewController(Browser* browser) instead.
  // TODO(crbug.com/1085419): Remove this interface after rolling the downstream
  // change.
  virtual UIViewController* NewFeedViewController(
      id<ApplicationCommands> handler) NS_RETURNS_RETAINED;
  // Returns the Discover Feed ViewController.
  virtual UIViewController* NewFeedViewController(Browser* browser)
      NS_RETURNS_RETAINED;
  // Updates the feed's theme to match the user's theme (light/dark).
  virtual void UpdateTheme();
  // Refreshes the Discover Feed with completion.
  virtual void RefreshFeedWithCompletion(ProceduralBlock completion);
};

#endif  // IOS_PUBLIC_PROVIDER_CHROME_BROWSER_DISCOVER_FEED_DISCOVER_FEED_PROVIDER_H_
