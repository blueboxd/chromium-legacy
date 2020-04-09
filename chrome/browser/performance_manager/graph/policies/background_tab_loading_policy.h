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
class BackgroundTabLoadingPolicy : public GraphOwned,
                                   public PageNode::ObserverDefaultImpl {
 public:
  BackgroundTabLoadingPolicy();
  ~BackgroundTabLoadingPolicy() override;
  BackgroundTabLoadingPolicy(const BackgroundTabLoadingPolicy& other) = delete;
  BackgroundTabLoadingPolicy& operator=(const BackgroundTabLoadingPolicy&) =
      delete;

  // GraphOwned implementation:
  void OnPassedToGraph(Graph* graph) override;
  void OnTakenFromGraph(Graph* graph) override;

  // PageNodeObserver implementation:
  void OnIsLoadingChanged(const PageNode* page_node) override;
  void OnBeforePageNodeRemoved(const PageNode* page_node) override;

  // Schedules the PageNodes in |page_nodes| to be loaded when appropriate.
  void ScheduleLoadForRestoredTabs(std::vector<PageNode*> page_nodes);

  void SetMockLoaderForTesting(std::unique_ptr<mechanism::PageLoader> loader);
  void SetMaxSimultaneousLoadsForTesting(size_t loading_slots);
  void SetFreeMemoryForTesting(size_t free_memory_mb);
  void ResetPolicyForTesting();

  // Returns the instance of BackgroundTabLoadingPolicy within the graph.
  static BackgroundTabLoadingPolicy* GetInstance();

 private:
  // Determines whether or not the given PageNode should be loaded. If this
  // returns false, then the policy no longer attempts to load |page_node| and
  // removes it from the policy's internal state. This is called immediately
  // prior to trying to load the PageNode.
  bool ShouldLoad(const PageNode* page_node);

  // Move the PageNode from |page_nodes_to_load_| to
  // |page_nodes_load_initiated_| and make the call to load the PageNode.
  void InitiateLoad(const PageNode* page_node);

  // Removes the PageNode from all the sets of PageNodes that the policy is
  // tracking.
  void RemovePageNode(const PageNode* page_node);

  // Initiates the load of enough tabs to fill all loading slots. No-ops if all
  // loading slots are occupied.
  void MaybeLoadSomeTabs();

  // Determines the number of tab loads that can be started at the moment to
  // avoid exceeding the number of loading slots.
  size_t GetMaxNewTabLoads() const;

  // Loads the next tab. This should only be called if there is a next tab to
  // load. This will always start loading a next tab even if the number of
  // simultaneously loading tabs is exceeded.
  void LoadNextTab();

  // Compute the amount of free memory on the system.
  size_t GetFreePhysicalMemoryMib() const;

  // The mechanism used to load the pages.
  std::unique_ptr<performance_manager::mechanism::PageLoader> page_loader_;

  // The set of PageNodes that have been restored for which we need to schedule
  // loads.
  std::vector<const PageNode*> page_nodes_to_load_;

  // The set of PageNodes that BackgroundTabLoadingPolicy has initiated loading,
  // and for which we are waiting for the loading to actually start. This signal
  // will be received from |OnIsLoadingChanged|.
  std::vector<const PageNode*> page_nodes_load_initiated_;

  // The set of PageNodes that are currently loading.
  std::vector<const PageNode*> page_nodes_loading_;

  // The number of simultaneous tab loads that are permitted by policy. This
  // is computed based on the number of cores on the machine.
  size_t max_simultaneous_tab_loads_;

  // The number of tab loads that have started. Every call to InitiateLoad
  // increments this value.
  size_t tab_loads_started_ = 0;

  // Used to overwrite the amount of free memory available on the system.
  size_t free_memory_mb_for_testing_ = 0;

  // The minimum total number of restored tabs to load.
  static constexpr uint32_t kMinTabsToLoad = 4;

  // The maximum total number of restored tabs to load.
  static constexpr uint32_t kMaxTabsToLoad = 20;

  // The minimum amount of memory to keep free.
  static constexpr uint32_t kDesiredAmountOfFreeMemoryMb = 150;

  // The maximum time since last use of a tab in order for it to be loaded.
  static constexpr base::TimeDelta kMaxTimeSinceLastUseToLoad =
      base::TimeDelta::FromDays(30);

  // Lower bound for the maximum number of tabs to load simultaneously.
  static constexpr uint32_t kMinSimultaneousTabLoads = 1;

  // Upper bound for the maximum number of tabs to load simultaneously.
  static constexpr uint32_t kMaxSimultaneousTabLoads = 4;

  // The number of CPU cores required per permitted simultaneous tab
  // load.
  static constexpr uint32_t kCoresPerSimultaneousTabLoad = 2;

  FRIEND_TEST_ALL_PREFIXES(BackgroundTabLoadingPolicyTest,
                           ShouldLoad_MaxTabsToRestore);
  FRIEND_TEST_ALL_PREFIXES(BackgroundTabLoadingPolicyTest,
                           ShouldLoad_MinTabsToRestore);
  FRIEND_TEST_ALL_PREFIXES(BackgroundTabLoadingPolicyTest,
                           ShouldLoad_FreeMemory);
  FRIEND_TEST_ALL_PREFIXES(BackgroundTabLoadingPolicyTest, ShouldLoad_OldTab);
};

}  // namespace policies

}  // namespace performance_manager

#endif  // CHROME_BROWSER_PERFORMANCE_MANAGER_GRAPH_POLICIES_BACKGROUND_TAB_LOADING_POLICY_H_
