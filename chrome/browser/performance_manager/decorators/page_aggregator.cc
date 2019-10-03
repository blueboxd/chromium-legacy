// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/performance_manager/decorators/page_aggregator.h"

#include <stdint.h>

#include "chrome/browser/performance_manager/graph/node_attached_data_impl.h"
#include "chrome/browser/performance_manager/graph/page_node_impl.h"
#include "services/resource_coordinator/public/mojom/coordination_unit.mojom.h"

namespace performance_manager {

using resource_coordinator::mojom::InterventionPolicy;

// Provides PageAggregator machinery access to some internals
// of a PageNodeImpl.
class PageAggregatorAccess {
 public:
  using StorageType = decltype(PageNodeImpl::page_aggregator_data_);

  static StorageType* GetInternalStorage(PageNodeImpl* page_node) {
    return &page_node->page_aggregator_data_;
  }

  static void SetOriginTrialFreezePolicy(PageNodeImpl* page_node,
                                         InterventionPolicy policy) {
    page_node->SetOriginTrialFreezePolicy(policy);
  }
};

class PageAggregator::Data : public NodeAttachedDataImpl<Data> {
 public:
  using StorageType = PageAggregatorAccess::StorageType;
  struct Traits : public NodeAttachedDataInternalOnNodeType<PageNodeImpl> {};

  explicit Data(const PageNodeImpl* page_node) {}
  ~Data() override = default;

  static StorageType* GetInternalStorage(PageNodeImpl* page_node) {
    return PageAggregatorAccess::GetInternalStorage(page_node);
  }

  void IncrementFrameCountForFreezingPolicy(InterventionPolicy policy) {
    ++num_current_frames_for_freezing_policy[static_cast<size_t>(policy)];
  }

  void DecrementFrameCountForFreezingPolicy(InterventionPolicy policy) {
    DCHECK_GT(
        num_current_frames_for_freezing_policy[static_cast<size_t>(policy)],
        0U);
    --num_current_frames_for_freezing_policy[static_cast<size_t>(policy)];
  }

  // Updates the page's origin trial freeze policy from current data.
  void UpdateOriginTrialFreezePolicy(PageNodeImpl* page_node) {
    PageAggregatorAccess::SetOriginTrialFreezePolicy(
        page_node, ComputeOriginTrialFreezePolicy());
  }

 private:
  // Computes the page's origin trial freeze policy from current data.
  InterventionPolicy ComputeOriginTrialFreezePolicy() const {
    if (GetNumCurrentFramesForFreezingPolicy(InterventionPolicy::kUnknown))
      return InterventionPolicy::kUnknown;

    if (GetNumCurrentFramesForFreezingPolicy(InterventionPolicy::kOptOut))
      return InterventionPolicy::kOptOut;

    if (GetNumCurrentFramesForFreezingPolicy(InterventionPolicy::kOptIn))
      return InterventionPolicy::kOptIn;

    // A page with no frame can be frozen. This will have no effect.
    return InterventionPolicy::kDefault;
  }

  // Returns the number of current frames with |policy| on the page that owns
  // this Data.
  uint32_t GetNumCurrentFramesForFreezingPolicy(
      InterventionPolicy policy) const {
    return num_current_frames_for_freezing_policy[static_cast<size_t>(policy)];
  }

  // The number of current frames of this page that has set each freeze origin
  // trial policy.
  uint32_t num_current_frames_for_freezing_policy
      [static_cast<size_t>(InterventionPolicy::kMaxValue) + 1] = {};

  DISALLOW_COPY_AND_ASSIGN(Data);
};

PageAggregator::PageAggregator() = default;
PageAggregator::~PageAggregator() = default;

void PageAggregator::OnFrameNodeAdded(const FrameNode* frame_node) {
  DCHECK(!frame_node->IsCurrent());
}

void PageAggregator::OnBeforeFrameNodeRemoved(const FrameNode* frame_node) {
  if (frame_node->IsCurrent()) {
    auto* page_node = PageNodeImpl::FromNode(frame_node->GetPageNode());
    Data* data = Data::Get(page_node);
    // Data should have been created when the frame became current.
    DCHECK(data);
    data->DecrementFrameCountForFreezingPolicy(
        frame_node->GetOriginTrialFreezePolicy());
    data->UpdateOriginTrialFreezePolicy(page_node);
  }
}

void PageAggregator::OnIsCurrentChanged(const FrameNode* frame_node) {
  auto* page_node = PageNodeImpl::FromNode(frame_node->GetPageNode());
  Data* data = Data::GetOrCreate(page_node);
  if (frame_node->IsCurrent()) {
    data->IncrementFrameCountForFreezingPolicy(
        frame_node->GetOriginTrialFreezePolicy());
  } else {
    data->DecrementFrameCountForFreezingPolicy(
        frame_node->GetOriginTrialFreezePolicy());
  }
  data->UpdateOriginTrialFreezePolicy(page_node);
}

void PageAggregator::OnOriginTrialFreezePolicyChanged(
    const FrameNode* frame_node,
    const InterventionPolicy& previous_value) {
  if (frame_node->IsCurrent()) {
    auto* page_node = PageNodeImpl::FromNode(frame_node->GetPageNode());
    Data* data = Data::Get(page_node);
    // Data should have been created when the frame became current.
    DCHECK(data);
    data->DecrementFrameCountForFreezingPolicy(previous_value);
    data->IncrementFrameCountForFreezingPolicy(
        frame_node->GetOriginTrialFreezePolicy());
    data->UpdateOriginTrialFreezePolicy(page_node);
  }
}

void PageAggregator::OnPassedToGraph(Graph* graph) {
  // This observer presumes that it's been added before any nodes exist in the
  // graph.
  DCHECK(GraphImpl::FromGraph(graph)->nodes().empty());
  graph->AddFrameNodeObserver(this);
}

void PageAggregator::OnTakenFromGraph(Graph* graph) {
  graph->RemoveFrameNodeObserver(this);
}

}  // namespace performance_manager
