// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "weblayer/browser/browser_list.h"

#include <algorithm>
#include <functional>

#include "weblayer/browser/browser_impl.h"
#include "weblayer/browser/browser_list_observer.h"

namespace weblayer {

// static
BrowserList* BrowserList::GetInstance() {
  static base::NoDestructor<BrowserList> browser_list;
  return browser_list.get();
}

#if defined(OS_ANDROID)
bool BrowserList::HasAtLeastOneResumedBrowser() {
  return std::any_of(browsers_.begin(), browsers_.end(),
                     std::mem_fn(&BrowserImpl::fragment_resumed));
}
#endif

void BrowserList::AddObserver(BrowserListObserver* observer) {
  observers_.AddObserver(observer);
}

void BrowserList::RemoveObserver(BrowserListObserver* observer) {
  observers_.RemoveObserver(observer);
}

BrowserList::BrowserList() = default;

BrowserList::~BrowserList() = default;

void BrowserList::AddBrowser(BrowserImpl* browser) {
  DCHECK(!browsers_.contains(browser));
#if defined(OS_ANDROID)
  // Browsers should not start out resumed.
  DCHECK(!browser->fragment_resumed());
#endif
  browsers_.insert(browser);
}

void BrowserList::RemoveBrowser(BrowserImpl* browser) {
  DCHECK(browsers_.contains(browser));
#if defined(OS_ANDROID)
  // Browsers should not be resumed when being destroyed.
  DCHECK(!browser->fragment_resumed());
#endif
  browsers_.erase(browser);
}

#if defined(OS_ANDROID)
void BrowserList::NotifyHasAtLeastOneResumedBrowserChanged() {
  const bool value = HasAtLeastOneResumedBrowser();
  for (BrowserListObserver& observer : observers_)
    observer.OnHasAtLeastOneResumedBrowserStateChanged(value);
}
#endif

}  // namespace weblayer
