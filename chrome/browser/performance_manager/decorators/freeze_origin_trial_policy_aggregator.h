// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PERFORMANCE_MANAGER_DECORATORS_FREEZE_ORIGIN_TRIAL_POLICY_AGGREGATOR_H_
#define CHROME_BROWSER_PERFORMANCE_MANAGER_DECORATORS_FREEZE_ORIGIN_TRIAL_POLICY_AGGREGATOR_H_

#include "chrome/browser/performance_manager/public/graph/frame_node.h"
#include "chrome/browser/performance_manager/public/graph/graph.h"

namespace performance_manager {

// Computes the freeze origin trial policy of a page by aggregating the freeze
// origin trial policies of its current frames.
class FreezeOriginTrialPolicyAggregator : public FrameNode::ObserverDefaultImpl,
                                          public GraphOwnedDefaultImpl {
 public:
  FreezeOriginTrialPolicyAggregator();
  ~FreezeOriginTrialPolicyAggregator() override;

 private:
  class Data;

  // FrameNodeObserver implementation:
  void OnFrameNodeAdded(const FrameNode* frame_node) override;
  void OnBeforeFrameNodeRemoved(const FrameNode* frame_node) override;
  void OnIsCurrentChanged(const FrameNode* frame_node) override;
  void OnOriginTrialFreezePolicyChanged(
      const FrameNode* frame_node,
      const InterventionPolicy& previous_value) override;

  // GraphOwned implementation:
  void OnPassedToGraph(Graph* graph) override;
  void OnTakenFromGraph(Graph* graph) override;

  DISALLOW_COPY_AND_ASSIGN(FreezeOriginTrialPolicyAggregator);
};

}  // namespace performance_manager

#endif  // CHROME_BROWSER_PERFORMANCE_MANAGER_DECORATORS_FREEZE_ORIGIN_TRIAL_POLICY_AGGREGATOR_H_
