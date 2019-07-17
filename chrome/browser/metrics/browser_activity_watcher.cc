// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/metrics/browser_activity_watcher.h"

#include "chrome/browser/ui/browser_list.h"
#include "chrome/browser/ui/browser_list_observer.h"

BrowserActivityWatcher::BrowserActivityWatcher(
    const base::RepeatingClosure& on_browser_list_change)
    : on_browser_list_change_(on_browser_list_change) {
  BrowserList::AddObserver(this);
}

BrowserActivityWatcher::~BrowserActivityWatcher() {
  BrowserList::RemoveObserver(this);
}

void BrowserActivityWatcher::OnBrowserAdded(Browser* browser) {
  on_browser_list_change_.Run();
}

void BrowserActivityWatcher::OnBrowserRemoved(Browser* browser) {
  on_browser_list_change_.Run();
}
