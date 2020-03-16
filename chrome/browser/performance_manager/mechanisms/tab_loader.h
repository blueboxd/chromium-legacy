// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PERFORMANCE_MANAGER_MECHANISMS_TAB_LOADER_H_
#define CHROME_BROWSER_PERFORMANCE_MANAGER_MECHANISMS_TAB_LOADER_H_

#include "base/macros.h"

namespace performance_manager {

class PageNode;

namespace mechanism {

// Mechanism that allows loading of the tab associated with a PageNode.
class TabLoader {
 public:
  TabLoader() = default;
  virtual ~TabLoader() = default;
  TabLoader(const TabLoader& other) = delete;
  TabLoader& operator=(const TabLoader&) = delete;

  // Starts loading |page_node| if not already loaded.
  virtual void LoadPageNode(const PageNode* page_node) {}
};

}  // namespace mechanism

}  // namespace performance_manager

#endif  // CHROME_BROWSER_PERFORMANCE_MANAGER_MECHANISMS_TAB_LOADER_H_
