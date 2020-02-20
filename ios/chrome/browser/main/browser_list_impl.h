// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_MAIN_BROWSER_LIST_IMPL_H_
#define IOS_CHROME_BROWSER_MAIN_BROWSER_LIST_IMPL_H_

#include "base/macros.h"
#import "ios/chrome/browser/main/browser_list.h"
#include "ios/chrome/browser/main/browser_list_observer.h"

// The concrete implementation of BrowserList returned by the
// BrowserListFactory.
class BrowserListImpl : public BrowserList {
 public:
  BrowserListImpl();
  ~BrowserListImpl() override;

  // BrowserList:
  void AddBrowser(Browser* browser) override;
  void AddIncognitoBrowser(Browser* browser) override;
  void RemoveBrowser(Browser* browser) override;
  void RemoveIncognitoBrowser(Browser* browser) override;
  std::set<Browser*> AllRegularBrowsers() const override;
  std::set<Browser*> AllIncognitoBrowsers() const override;
  void AddObserver(BrowserListObserver* observer) override;
  void RemoveObserver(BrowserListObserver* observer) override;

 private:
  std::set<Browser*> browsers_;
  std::set<Browser*> incognito_browsers_;
  base::ObserverList<BrowserListObserver, true>::Unchecked observers_;

  DISALLOW_COPY_AND_ASSIGN(BrowserListImpl);
};

#endif  // IOS_CHROME_BROWSER_MAIN_BROWSER_LIST_IMPL_H_
