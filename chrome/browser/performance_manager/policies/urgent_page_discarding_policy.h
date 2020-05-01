// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PERFORMANCE_MANAGER_POLICIES_URGENT_PAGE_DISCARDING_POLICY_H_
#define CHROME_BROWSER_PERFORMANCE_MANAGER_POLICIES_URGENT_PAGE_DISCARDING_POLICY_H_

#include "base/containers/flat_map.h"
#include "base/macros.h"
#include "base/memory/memory_pressure_listener.h"
#include "base/memory/weak_ptr.h"
#include "base/optional.h"
#include "base/sequence_checker.h"
#include "components/performance_manager/public/decorators/page_live_state_decorator.h"
#include "components/performance_manager/public/graph/graph.h"
#include "components/performance_manager/public/graph/node_data_describer.h"
#include "components/performance_manager/public/graph/page_node.h"

namespace performance_manager {

namespace mechanism {
class PageDiscarder;
}  // namespace mechanism

namespace policies {

// Urgently discard a tab when receiving a memory pressure signal. The discarded
// tab will be the eligible tab with the largest resident set.
class UrgentPageDiscardingPolicy : public GraphOwned,
                                   public PageNode::ObserverDefaultImpl,
                                   public NodeDataDescriberDefaultImpl {
 public:
  UrgentPageDiscardingPolicy();
  ~UrgentPageDiscardingPolicy() override;
  UrgentPageDiscardingPolicy(const UrgentPageDiscardingPolicy& other) = delete;
  UrgentPageDiscardingPolicy& operator=(const UrgentPageDiscardingPolicy&) =
      delete;

  // GraphOwned implementation:
  void OnPassedToGraph(Graph* graph) override;
  void OnTakenFromGraph(Graph* graph) override;

  // PageNodeObserver:
  void OnBeforePageNodeRemoved(const PageNode* page_node) override;
  void OnIsAudibleChanged(const PageNode* page_node) override;

  void SetMockDiscarderForTesting(
      std::unique_ptr<mechanism::PageDiscarder> discarder);
  bool CanUrgentlyDiscardForTesting(const PageNode* page_node) const {
    return CanUrgentlyDiscard(page_node);
  }
  void AdornsPageWithDiscardAttemptMarkerForTesting(PageNode* page_node);

 protected:
  // Returns the PageLiveStateDecorator::Data associated with a PageNode.
  // Exposed and made virtual to allowed injecting some fake data in tests.
  virtual const PageLiveStateDecorator::Data* GetPageNodeLiveStateData(
      const PageNode* page_node) const;

 private:
  void OnMemoryPressure(
      base::MemoryPressureListener::MemoryPressureLevel level);

  // Register to start listening to memory pressure. Called on startup or after
  // handling a pressure event.
  void RegisterMemoryPressureListener();

  // Unregister to stop listening to memory pressure. Called on shutdown or
  // when handling a pressure event.
  void UnregisterMemoryPressureListener();

  // Indicates if a PageNode can be urgently discarded.
  bool CanUrgentlyDiscard(const PageNode* page_node) const;

  // Selects a tab to discard and posts to the UI thread to discard it.
  void UrgentlyDiscardAPage();

  // Callback called when a discard attempt has completed.
  void PostDiscardAttemptCallback(bool success);

  // NodeDataDescriber implementation:
  base::Value DescribePageNodeData(const PageNode* node) const override;

  std::unique_ptr<base::MemoryPressureListener> memory_pressure_listener_;
  Graph* graph_ = nullptr;

  // Map that associates a PageNode with the last time it became non audible.
  // PageNodes that have never been audible are not present in this map.
  base::flat_map<const PageNode*, base::TimeTicks>
      last_change_to_non_audible_time_;

  std::unique_ptr<performance_manager::mechanism::PageDiscarder>
      page_discarder_;

  SEQUENCE_CHECKER(sequence_checker_);

  base::WeakPtrFactory<UrgentPageDiscardingPolicy> weak_factory_{this};
};

}  // namespace policies
}  // namespace performance_manager

#endif  // CHROME_BROWSER_PERFORMANCE_MANAGER_POLICIES_URGENT_PAGE_DISCARDING_POLICY_H_
