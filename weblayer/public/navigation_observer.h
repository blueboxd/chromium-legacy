// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WEBLAYER_PUBLIC_NAVIGATION_OBSERVER_H_
#define WEBLAYER_PUBLIC_NAVIGATION_OBSERVER_H_

#include <algorithm>

namespace weblayer {
class Navigation;

// The lifecycle of a navigation:
// 1) NavigationStarted
// 2) 0 or more NavigationRedirected
// 3) 0 or 1 NavigationCommitted
// 3) 0 or 1 NavigationHadFirstContentfulPaint
// 4) NavigationCompleted or NavigationFailed
class NavigationObserver {
 public:
  virtual ~NavigationObserver() {}

  virtual void NavigationStarted(Navigation* navigation) = 0;

  virtual void NavigationRedirected(Navigation* navigation) = 0;

  virtual void NavigationCommitted(Navigation* navigation) = 0;

  virtual void NavigationHadFirstContentfulPaint(Navigation* navigation) = 0;

  virtual void NavigationCompleted(Navigation* navigation) = 0;

  virtual void NavigationFailed(Navigation* navigation) = 0;
};

}  // namespace weblayer

#endif  // WEBLAYER_PUBLIC_NAVIGATION_OBSERVER_H_
