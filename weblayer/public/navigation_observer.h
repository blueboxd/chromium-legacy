// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WEBLAYER_PUBLIC_NAVIGATION_OBSERVER_H_
#define WEBLAYER_PUBLIC_NAVIGATION_OBSERVER_H_

namespace weblayer {
class Navigation;

// An interface for a WebLayer embedder to get notified about navigations. For
// now this only notifies for the main frame.
//
// The lifecycle of a navigation:
// 1) NavigationStarted
// 2) 0 or more NavigationRedirected
// 3) 0 or 1 NavigationCommitted
// 4) NavigationCompleted or NavigationFailed
class NavigationObserver {
 public:
  virtual ~NavigationObserver() {}

  virtual void NavigationStarted(Navigation* navigation) {}

  virtual void NavigationRedirected(Navigation* navigation) {}

  virtual void NavigationCommitted(Navigation* navigation) {}

  virtual void NavigationCompleted(Navigation* navigation) {}

  virtual void NavigationFailed(Navigation* navigation) {}
};

}  // namespace weblayer

#endif  // WEBLAYER_PUBLIC_NAVIGATION_OBSERVER_H_
