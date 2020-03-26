// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PERFORMANCE_MANAGER_GRAPH_POLICIES_BACKGROUND_TAB_LOADING_POLICY_H_
#define CHROME_BROWSER_PERFORMANCE_MANAGER_GRAPH_POLICIES_BACKGROUND_TAB_LOADING_POLICY_H_

#include <vector>

#include "components/performance_manager/public/graph/graph.h"
#include "components/performance_manager/public/graph/page_node.h"

namespace performance_manager {

namespace mechanism {
class PageLoader;
}  // namespace mechanism

namespace policies {

// This policy manages loading of background tabs created by session restore. It
// is responsible for assigning priorities and controlling the load of
// background tab loading at all times.
class BackgroundTabLoadingPolicy : public GraphOwned {
 public:
  BackgroundTabLoadingPolicy();
  ~BackgroundTabLoadingPolicy() override;
  BackgroundTabLoadingPolicy(const BackgroundTabLoadingPolicy& other) = delete;
  BackgroundTabLoadingPolicy& operator=(const BackgroundTabLoadingPolicy&) =
      delete;

  // GraphOwned implementation:
  void OnPassedToGraph(Graph* graph) override;
  void OnTakenFromGraph(Graph* graph) override;

  // Schedules the PageNodes in |page_nodes| to be loaded when appropriate.
  void ScheduleLoadForRestoredTabs(std::vector<PageNode*> page_nodes);

  void SetMockLoaderForTesting(std::unique_ptr<mechanism::PageLoader> loader);

  // Returns the instance of BackgroundTabLoadingPolicy within the graph.
  static BackgroundTabLoadingPolicy* GetInstance();

 private:
  std::unique_ptr<performance_manager::mechanism::PageLoader> page_loader_;
};

}  // namespace policies

}  // namespace performance_manager

#endif  // CHROME_BROWSER_PERFORMANCE_MANAGER_GRAPH_POLICIES_BACKGROUND_TAB_LOADING_POLICY_H_
