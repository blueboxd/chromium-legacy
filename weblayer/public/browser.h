// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WEBLAYER_PUBLIC_BROWSER_H_
#define WEBLAYER_PUBLIC_BROWSER_H_

#include <memory>
#include <vector>

namespace weblayer {

class BrowserObserver;
class Profile;
class Tab;

// Represents an ordered list of Tabs, with one active. Browser does not own
// the set of Tabs.
class Browser {
 public:
  static std::unique_ptr<Browser> Create(Profile* profile);

  virtual ~Browser() {}

  virtual void AddTab(Tab* tab) = 0;
  virtual void RemoveTab(Tab* tab) = 0;
  virtual void SetActiveTab(Tab* tab) = 0;
  virtual Tab* GetActiveTab() = 0;
  virtual const std::vector<Tab*>& GetTabs() = 0;

  virtual void AddObserver(BrowserObserver* observer) = 0;
  virtual void RemoveObserver(BrowserObserver* observer) = 0;
};

}  // namespace weblayer

#endif  // WEBLAYER_PUBLIC_BROWSER_H_
