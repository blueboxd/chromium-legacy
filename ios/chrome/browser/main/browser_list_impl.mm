// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/main/browser_list_impl.h"

#include "ios/chrome/browser/browser_state/chrome_browser_state.h"
#import "ios/chrome/browser/main/browser.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

BrowserListImpl::BrowserListImpl() {}
BrowserListImpl::~BrowserListImpl() {}

// BrowserList:
void BrowserListImpl::AddBrowser(Browser* browser) {
  DCHECK(!browser->GetBrowserState()->IsOffTheRecord());
  browsers_.insert(browser);
  for (auto& observer : observers_)
    observer.OnBrowserAdded(this, browser);
}

void BrowserListImpl::AddIncognitoBrowser(Browser* browser) {
  DCHECK(browser->GetBrowserState()->IsOffTheRecord());
  incognito_browsers_.insert(browser);
  for (auto& observer : observers_)
    observer.OnIncognitoBrowserAdded(this, browser);
}

void BrowserListImpl::RemoveBrowser(Browser* browser) {
  if (browsers_.erase(browser) > 0) {
    for (auto& observer : observers_)
      observer.OnBrowserRemoved(this, browser);
  }
}

void BrowserListImpl::RemoveIncognitoBrowser(Browser* browser) {
  if (incognito_browsers_.erase(browser) > 0) {
    for (auto& observer : observers_)
      observer.OnIncognitoBrowserRemoved(this, browser);
  }
}

std::set<Browser*> BrowserListImpl::AllRegularBrowsers() const {
  return browsers_;
}

std::set<Browser*> BrowserListImpl::AllIncognitoBrowsers() const {
  return incognito_browsers_;
}

// Adds an observer to the model.
void BrowserListImpl::AddObserver(BrowserListObserver* observer) {
  observers_.AddObserver(observer);
}

// Removes an observer from the model.
void BrowserListImpl::RemoveObserver(BrowserListObserver* observer) {
  observers_.RemoveObserver(observer);
}
