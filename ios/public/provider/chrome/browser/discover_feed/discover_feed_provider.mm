// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/public/provider/chrome/browser/discover_feed/discover_feed_provider.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

bool DiscoverFeedProvider::IsDiscoverFeedEnabled() {
  return false;
}

UIViewController* DiscoverFeedProvider::NewFeedViewController(
    id<ApplicationCommands> dispatcher) {
  return nil;
}

void DiscoverFeedProvider::RefreshFeedWithCompletion(
    ProceduralBlock completion) {}
