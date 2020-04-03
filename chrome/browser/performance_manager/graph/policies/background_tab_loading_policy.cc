// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/performance_manager/graph/policies/background_tab_loading_policy.h"

#include "base/system/sys_info.h"
#include "chrome/browser/performance_manager/graph/policies/background_tab_loading_policy_helpers.h"
#include "chrome/browser/performance_manager/mechanisms/page_loader.h"
#include "components/performance_manager/public/decorators/tab_properties_decorator.h"
#include "components/performance_manager/public/graph/policies/background_tab_loading_policy.h"
#include "components/performance_manager/public/performance_manager.h"

namespace performance_manager {

namespace policies {

namespace {

// Pointer to the instance of itself.
BackgroundTabLoadingPolicy* g_background_tab_loading_policy = nullptr;

// Lower bound for the maximum number of tabs to load simultaneously.
uint32_t kMinSimultaneousTabLoads = 1;

// Upper bound for the maximum number of tabs to load simultaneously.
// Setting to zero means no upper bound is applied.
uint32_t kMaxSimultaneousTabLoads = 4;

// The number of CPU cores required per permitted simultaneous tab
// load. Setting to zero means no CPU core limit applies.
uint32_t kCoresPerSimultaneousTabLoad = 2;

}  // namespace

void ScheduleLoadForRestoredTabs(
    std::vector<content::WebContents*> web_contents_vector) {
  std::vector<base::WeakPtr<PageNode>> weakptr_page_nodes;
  weakptr_page_nodes.reserve(web_contents_vector.size());
  for (auto* content : web_contents_vector) {
    weakptr_page_nodes.push_back(
        PerformanceManager::GetPageNodeForWebContents(content));
  }
  performance_manager::PerformanceManager::CallOnGraph(
      FROM_HERE, base::BindOnce(
                     [](std::vector<base::WeakPtr<PageNode>> weakptr_page_nodes,
                        performance_manager::Graph* graph) {
                       std::vector<PageNode*> page_nodes;
                       page_nodes.reserve(weakptr_page_nodes.size());
                       for (auto page_node : weakptr_page_nodes) {
                         // If the PageNode has been deleted before
                         // BackgroundTabLoading starts restoring it, then there
                         // is no need to restore it.
                         if (PageNode* raw_page = page_node.get())
                           page_nodes.push_back(raw_page);
                       }
                       BackgroundTabLoadingPolicy::GetInstance()
                           ->ScheduleLoadForRestoredTabs(std::move(page_nodes));
                     },
                     std::move(weakptr_page_nodes)));
}

BackgroundTabLoadingPolicy::BackgroundTabLoadingPolicy()
    : page_loader_(std::make_unique<mechanism::PageLoader>()) {
  DCHECK(!g_background_tab_loading_policy);
  g_background_tab_loading_policy = this;
  max_simultaneous_tab_loads_ = CalculateMaxSimultaneousTabLoads(
      kMinSimultaneousTabLoads, kMaxSimultaneousTabLoads,
      kCoresPerSimultaneousTabLoad, base::SysInfo::NumberOfProcessors());
}

BackgroundTabLoadingPolicy::~BackgroundTabLoadingPolicy() {
  DCHECK_EQ(this, g_background_tab_loading_policy);
  g_background_tab_loading_policy = nullptr;
}

void BackgroundTabLoadingPolicy::OnPassedToGraph(Graph* graph) {
  graph->AddPageNodeObserver(this);
}

void BackgroundTabLoadingPolicy::OnTakenFromGraph(Graph* graph) {
  graph->RemovePageNodeObserver(this);
}

void BackgroundTabLoadingPolicy::OnIsLoadingChanged(const PageNode* page_node) {
  if (!page_node->IsLoading()) {
    // Once the PageNode finishes loading, stop tracking it within this policy.
    RemovePageNode(page_node);

    // Since there is a free loading slot, load more tab if needed.
    MaybeLoadSomeTabs();
    return;
  }
  // The PageNode started loading, either because of this policy or because of
  // external factors (e.g. user-initiated). In either case, remove the PageNode
  // from the set of PageNodes for which a load needs to be initiated and from
  // the set of PageNodes for which a load has been initiated but hasn't
  // started.
  base::Erase(page_nodes_to_load_, page_node);
  base::Erase(page_nodes_load_initiated_, page_node);

  // Keep track of all PageNodes that are loading, even when the load isn't
  // initiated by this policy.
  DCHECK(!base::Contains(page_nodes_loading_, page_node));
  page_nodes_loading_.push_back(page_node);
}

void BackgroundTabLoadingPolicy::OnBeforePageNodeRemoved(
    const PageNode* page_node) {
  RemovePageNode(page_node);

  // There may be free loading slots, check and load more tabs if that's the
  // case.
  MaybeLoadSomeTabs();
}

void BackgroundTabLoadingPolicy::ScheduleLoadForRestoredTabs(
    std::vector<PageNode*> page_nodes) {
  for (auto* page_node : page_nodes) {
    // Put the |page_node| in the queue for loading.
    DCHECK(!base::Contains(page_nodes_to_load_, page_node));
    page_nodes_to_load_.push_back(page_node);
    DCHECK(
        TabPropertiesDecorator::Data::FromPageNode(page_node)->IsInTabStrip());
  }
  MaybeLoadSomeTabs();
}

void BackgroundTabLoadingPolicy::SetMockLoaderForTesting(
    std::unique_ptr<mechanism::PageLoader> loader) {
  page_loader_ = std::move(loader);
}

void BackgroundTabLoadingPolicy::SetMaxSimultaneousLoadsForTesting(
    size_t loading_slots) {
  max_simultaneous_tab_loads_ = loading_slots;
}

BackgroundTabLoadingPolicy* BackgroundTabLoadingPolicy::GetInstance() {
  return g_background_tab_loading_policy;
}

void BackgroundTabLoadingPolicy::InitiateLoad(const PageNode* page_node) {
  // Mark |page_node| as load initiated. Ensure that InitiateLoad is only called
  // for a PageNode that is tracked by the policy.
  size_t num_removed = base::Erase(page_nodes_to_load_, page_node);
  DCHECK_EQ(num_removed, 1U);
  page_nodes_load_initiated_.push_back(page_node);

  // Make the call to load |page_node|.
  page_loader_->LoadPageNode(page_node);
}

void BackgroundTabLoadingPolicy::RemovePageNode(const PageNode* page_node) {
  base::Erase(page_nodes_to_load_, page_node);
  base::Erase(page_nodes_load_initiated_, page_node);
  base::Erase(page_nodes_loading_, page_node);
}

void BackgroundTabLoadingPolicy::MaybeLoadSomeTabs() {
  // Continue to load tabs while possible. This is in a loop with a
  // recalculation of GetMaxNewTabLoads() as reentrancy can cause conditions
  // to change as each tab load is initiated.
  while (GetMaxNewTabLoads() > 0)
    LoadNextTab();
}

size_t BackgroundTabLoadingPolicy::GetMaxNewTabLoads() const {
  // This takes into account all tabs currently loading across the browser,
  // including ones that BackgroundTabLoadingPolicy isn't explicitly managing.
  // This ensures that BackgroundTabLoadingPolicy respects user interaction
  // first and foremost. There's a small race between when we initiated loading
  // and when PageNodeObserver notifies us that it has actually started, so we
  // also make use of |page_nodes_initiated_| to track these.
  size_t loading_tab_count =
      page_nodes_load_initiated_.size() + page_nodes_loading_.size();

  // Determine the number of free loading slots available.
  size_t page_nodes_to_load = 0;
  if (loading_tab_count < max_simultaneous_tab_loads_)
    page_nodes_to_load = max_simultaneous_tab_loads_ - loading_tab_count;

  // Cap the number of loads by the actual number of tabs remaining.
  page_nodes_to_load = std::min(page_nodes_to_load, page_nodes_to_load_.size());

  return page_nodes_to_load;
}

void BackgroundTabLoadingPolicy::LoadNextTab() {
  DCHECK(!page_nodes_to_load_.empty());

  // Find the next PageNode to load.
  const PageNode* page_node = page_nodes_to_load_.front();

  InitiateLoad(page_node);
}

}  // namespace policies

}  // namespace performance_manager
