// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_METRICS_BROWSER_ACTIVITY_WATCHER_H_
#define CHROME_BROWSER_METRICS_BROWSER_ACTIVITY_WATCHER_H_

#include "base/callback.h"
#include "base/macros.h"
#include "chrome/browser/ui/browser_list_observer.h"

// Helper that fires a callback every time a Browser object is added or removed
// from the BrowserList. This class primarily exists to encapsulate this
// behavior and reduce the number of platform-specific macros as Android doesn't
// use BrowserList.
class BrowserActivityWatcher : public BrowserListObserver {
 public:
  explicit BrowserActivityWatcher(
      const base::RepeatingClosure& on_browser_list_change);
  ~BrowserActivityWatcher() override;

  // BrowserListObserver:
  void OnBrowserAdded(Browser* browser) override;
  void OnBrowserRemoved(Browser* browser) override;

 private:
  base::RepeatingClosure on_browser_list_change_;

  DISALLOW_COPY_AND_ASSIGN(BrowserActivityWatcher);
};

#endif  // CHROME_BROWSER_METRICS_BROWSER_ACTIVITY_WATCHER_H_
