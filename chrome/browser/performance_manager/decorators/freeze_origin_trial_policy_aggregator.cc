// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/performance_manager/decorators/freeze_origin_trial_policy_aggregator.h"

#include <stdint.h>

#include "chrome/browser/performance_manager/graph/node_attached_data_impl.h"
#include "chrome/browser/performance_manager/graph/page_node_impl.h"
#include "services/resource_coordinator/public/mojom/coordination_unit.mojom.h"

namespace performance_manager {

using resource_coordinator::mojom::InterventionPolicy;

// Provides FreezeOriginTrialPolicyAggregator machinery access to some internals
// of a PageNodeImpl.
class FreezeOriginTrialPolicyAggregatorAccess {
 public:
  using StorageType = decltype(PageNodeImpl::freeze_origin_trial_policy_data_);

  static StorageType* GetInternalStorage(PageNodeImpl* page_node) {
    return &page_node->freeze_origin_trial_policy_data_;
  }

  static void SetOriginTrialFreezePolicy(PageNodeImpl* page_node,
                                         InterventionPolicy policy) {
    page_node->SetOriginTrialFreezePolicy(policy);
  }
};

class FreezeOriginTrialPolicyAggregator::Data
    : public NodeAttachedDataImpl<Data> {
 public:
  using StorageType = FreezeOriginTrialPolicyAggregatorAccess::StorageType;
  struct Traits : public NodeAttachedDataInternalOnNodeType<PageNodeImpl> {};

  explicit Data(const PageNodeImpl* page_node) {}
  ~Data() override = default;

  static StorageType* GetInternalStorage(PageNodeImpl* page_node) {
    return FreezeOriginTrialPolicyAggregatorAccess::GetInternalStorage(
        page_node);
  }

  void IncrementFrameCountForPolicy(InterventionPolicy policy) {
    ++num_current_frames_for_policy[static_cast<size_t>(policy)];
  }

  void DecrementFrameCountForPolicy(InterventionPolicy policy) {
    DCHECK_GT(num_current_frames_for_policy[static_cast<size_t>(policy)], 0U);
    --num_current_frames_for_policy[static_cast<size_t>(policy)];
  }

  // Updates the page's origin trial freeze policy from current data.
  void UpdateOriginTrialFreezePolicy(PageNodeImpl* page_node) {
    FreezeOriginTrialPolicyAggregatorAccess::SetOriginTrialFreezePolicy(
        page_node, ComputeOriginTrialFreezePolicy());
  }

 private:
  // Computes the page's origin trial freeze policy from current data.
  InterventionPolicy ComputeOriginTrialFreezePolicy() const {
    if (GetNumCurrentFramesForPolicy(InterventionPolicy::kUnknown))
      return InterventionPolicy::kUnknown;

    if (GetNumCurrentFramesForPolicy(InterventionPolicy::kOptOut))
      return InterventionPolicy::kOptOut;

    if (GetNumCurrentFramesForPolicy(InterventionPolicy::kOptIn))
      return InterventionPolicy::kOptIn;

    // A page with no frame can be frozen. This will have no effect.
    return InterventionPolicy::kDefault;
  }

  // Returns the number of current frames with |policy| on the page that owns
  // this Data.
  uint32_t GetNumCurrentFramesForPolicy(InterventionPolicy policy) const {
    return num_current_frames_for_policy[static_cast<size_t>(policy)];
  }

  // The number of current frames of this page that has set each freeze origin
  // trial policy.
  uint32_t num_current_frames_for_policy[static_cast<size_t>(
                                             InterventionPolicy::kMaxValue) +
                                         1] = {};

  DISALLOW_COPY_AND_ASSIGN(Data);
};

FreezeOriginTrialPolicyAggregator::FreezeOriginTrialPolicyAggregator() =
    default;
FreezeOriginTrialPolicyAggregator::~FreezeOriginTrialPolicyAggregator() =
    default;

void FreezeOriginTrialPolicyAggregator::OnFrameNodeAdded(
    const FrameNode* frame_node) {
  DCHECK(!frame_node->IsCurrent());
}

void FreezeOriginTrialPolicyAggregator::OnBeforeFrameNodeRemoved(
    const FrameNode* frame_node) {
  if (frame_node->IsCurrent()) {
    auto* page_node = PageNodeImpl::FromNode(frame_node->GetPageNode());
    Data* data = Data::Get(page_node);
    // Data should have been created when the frame became current.
    DCHECK(data);
    data->DecrementFrameCountForPolicy(
        frame_node->GetOriginTrialFreezePolicy());
    data->UpdateOriginTrialFreezePolicy(page_node);
  }
}

void FreezeOriginTrialPolicyAggregator::OnIsCurrentChanged(
    const FrameNode* frame_node) {
  auto* page_node = PageNodeImpl::FromNode(frame_node->GetPageNode());
  Data* data = Data::GetOrCreate(page_node);
  if (frame_node->IsCurrent()) {
    data->IncrementFrameCountForPolicy(
        frame_node->GetOriginTrialFreezePolicy());
  } else {
    data->DecrementFrameCountForPolicy(
        frame_node->GetOriginTrialFreezePolicy());
  }
  data->UpdateOriginTrialFreezePolicy(page_node);
}

void FreezeOriginTrialPolicyAggregator::OnOriginTrialFreezePolicyChanged(
    const FrameNode* frame_node,
    const InterventionPolicy& previous_value) {
  if (frame_node->IsCurrent()) {
    auto* page_node = PageNodeImpl::FromNode(frame_node->GetPageNode());
    Data* data = Data::Get(page_node);
    // Data should have been created when the frame became current.
    DCHECK(data);
    data->DecrementFrameCountForPolicy(previous_value);
    data->IncrementFrameCountForPolicy(
        frame_node->GetOriginTrialFreezePolicy());
    data->UpdateOriginTrialFreezePolicy(page_node);
  }
}

void FreezeOriginTrialPolicyAggregator::OnPassedToGraph(Graph* graph) {
  // This observer presumes that it's been added before any nodes exist in the
  // graph.
  DCHECK(GraphImpl::FromGraph(graph)->nodes().empty());
  graph->AddFrameNodeObserver(this);
}

void FreezeOriginTrialPolicyAggregator::OnTakenFromGraph(Graph* graph) {
  graph->RemoveFrameNodeObserver(this);
}

}  // namespace performance_manager
