// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/performance_manager/graph/page_node_impl.h"

#include <memory>

#include "base/logging.h"
#include "base/stl_util.h"
#include "base/time/default_tick_clock.h"
#include "chrome/browser/performance_manager/graph/frame_node_impl.h"
#include "chrome/browser/performance_manager/graph/graph_impl.h"
#include "chrome/browser/performance_manager/graph/graph_impl_operations.h"
#include "chrome/browser/performance_manager/graph/process_node_impl.h"
#include "chrome/browser/performance_manager/performance_manager_clock.h"

namespace performance_manager {

namespace {

constexpr size_t kMaxInterventionIndex = static_cast<size_t>(
    resource_coordinator::mojom::PolicyControlledIntervention::kMaxValue);

size_t ToIndex(
    resource_coordinator::mojom::PolicyControlledIntervention intervention) {
  const size_t kIndex = static_cast<size_t>(intervention);
  DCHECK(kIndex <= kMaxInterventionIndex);
  return kIndex;
}

}  // namespace

PageNodeImpl::PageNodeImpl(GraphImpl* graph,
                           const WebContentsProxy& contents_proxy,
                           bool is_visible,
                           bool is_audible)
    : TypedNodeBase(graph),
      contents_proxy_(contents_proxy),
      visibility_change_time_(PerformanceManagerClock::NowTicks()),
      is_visible_(is_visible),
      is_audible_(is_audible) {
  DETACH_FROM_SEQUENCE(sequence_checker_);
}

PageNodeImpl::~PageNodeImpl() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
}

const WebContentsProxy& PageNodeImpl::contents_proxy() const {
  return contents_proxy_;
}

void PageNodeImpl::AddFrame(FrameNodeImpl* frame_node) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(frame_node);
  DCHECK_EQ(this, frame_node->page_node());
  DCHECK(graph()->NodeInGraph(frame_node));

  ++frame_node_count_;
  if (frame_node->parent_frame_node() == nullptr)
    main_frame_nodes_.insert(frame_node);

  MaybeInvalidateInterventionPolicies(frame_node, true /* adding_frame */);
}

void PageNodeImpl::RemoveFrame(FrameNodeImpl* frame_node) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(frame_node);
  DCHECK_EQ(this, frame_node->page_node());
  DCHECK(graph()->NodeInGraph(frame_node));

  --frame_node_count_;
  if (frame_node->parent_frame_node() == nullptr) {
    size_t removed = main_frame_nodes_.erase(frame_node);
    DCHECK_EQ(1u, removed);
  }

  MaybeInvalidateInterventionPolicies(frame_node, false /* adding_frame */);
}

void PageNodeImpl::SetIsLoading(bool is_loading) {
  is_loading_.SetAndMaybeNotify(this, is_loading);
}

void PageNodeImpl::SetIsVisible(bool is_visible) {
  if (is_visible_.SetAndMaybeNotify(this, is_visible)) {
    // The change time needs to be updated after observers are notified, as they
    // use this to determine time passed since the *previous* visibility state
    // change. They can infer the current state change time themselves via
    // NowTicks.
    visibility_change_time_ = PerformanceManagerClock::NowTicks();
  }
}

void PageNodeImpl::SetIsAudible(bool is_audible) {
  is_audible_.SetAndMaybeNotify(this, is_audible);
}

void PageNodeImpl::SetUkmSourceId(ukm::SourceId ukm_source_id) {
  ukm_source_id_.SetAndMaybeNotify(this, ukm_source_id);
}

void PageNodeImpl::OnFaviconUpdated() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  for (auto& observer : observers())
    observer.OnFaviconUpdated(this);
  for (auto* observer : GetObservers())
    observer->OnFaviconUpdated(this);
}

void PageNodeImpl::OnTitleUpdated() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  for (auto& observer : observers())
    observer.OnTitleUpdated(this);
  for (auto* observer : GetObservers())
    observer->OnTitleUpdated(this);
}

void PageNodeImpl::OnMainFrameNavigationCommitted(
    base::TimeTicks navigation_committed_time,
    int64_t navigation_id,
    const GURL& url) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  navigation_committed_time_ = navigation_committed_time;
  main_frame_url_ = url;
  navigation_id_ = navigation_id;
  for (auto& observer : observers())
    observer.OnMainFrameNavigationCommitted(this);
  for (auto* observer : GetObservers())
    observer->OnMainFrameNavigationCommitted(this);
}

double PageNodeImpl::GetCPUUsage() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  double cpu_usage = 0;

  // TODO(chrisha/siggi): This should all be ripped out / refactored.
  for (auto* process_node :
       GraphImplOperations::GetAssociatedProcessNodes(this)) {
    size_t pages_in_process =
        GraphImplOperations::GetAssociatedPageNodes(process_node).size();
    DCHECK_LE(1u, pages_in_process);
    cpu_usage += process_node->cpu_usage() / pages_in_process;
  }

  return cpu_usage;
}

base::TimeDelta PageNodeImpl::TimeSinceLastNavigation() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (navigation_committed_time_.is_null())
    return base::TimeDelta();
  return PerformanceManagerClock::NowTicks() - navigation_committed_time_;
}

base::TimeDelta PageNodeImpl::TimeSinceLastVisibilityChange() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return PerformanceManagerClock::NowTicks() - visibility_change_time_;
}

FrameNodeImpl* PageNodeImpl::GetMainFrameNodeImpl() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (main_frame_nodes_.empty())
    return nullptr;

  // Return the current frame node if there is one. Iterating over this set is
  // fine because it is almost always of length 1 or 2.
  for (auto* frame : main_frame_nodes_) {
    if (frame->is_current())
      return frame;
  }

  // Otherwise, return any old main frame node.
  return *main_frame_nodes_.begin();
}

bool PageNodeImpl::is_visible() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return is_visible_.value();
}

bool PageNodeImpl::is_audible() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return is_audible_.value();
}

bool PageNodeImpl::is_loading() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return is_loading_.value();
}

ukm::SourceId PageNodeImpl::ukm_source_id() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return ukm_source_id_.value();
}

PageNodeImpl::LifecycleState PageNodeImpl::lifecycle_state() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return lifecycle_state_.value();
}

const base::flat_set<FrameNodeImpl*>& PageNodeImpl::main_frame_nodes() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return main_frame_nodes_;
}

base::TimeTicks PageNodeImpl::usage_estimate_time() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return usage_estimate_time_;
}

base::TimeDelta PageNodeImpl::cumulative_cpu_usage_estimate() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return cumulative_cpu_usage_estimate_;
}

uint64_t PageNodeImpl::private_footprint_kb_estimate() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return private_footprint_kb_estimate_;
}

bool PageNodeImpl::page_almost_idle() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return page_almost_idle_.value();
}

const GURL& PageNodeImpl::main_frame_url() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return main_frame_url_;
}

int64_t PageNodeImpl::navigation_id() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return navigation_id_;
}

void PageNodeImpl::set_usage_estimate_time(
    base::TimeTicks usage_estimate_time) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  usage_estimate_time_ = usage_estimate_time;
}

void PageNodeImpl::set_cumulative_cpu_usage_estimate(
    base::TimeDelta cumulative_cpu_usage_estimate) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  cumulative_cpu_usage_estimate_ = cumulative_cpu_usage_estimate;
}

void PageNodeImpl::set_private_footprint_kb_estimate(
    uint64_t private_footprint_kb_estimate) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  private_footprint_kb_estimate_ = private_footprint_kb_estimate;
}

void PageNodeImpl::set_has_nonempty_beforeunload(
    bool has_nonempty_beforeunload) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  has_nonempty_beforeunload_ = has_nonempty_beforeunload;
}

void PageNodeImpl::OnFrameInterventionPolicyChanged(
    FrameNodeImpl* frame,
    resource_coordinator::mojom::PolicyControlledIntervention intervention,
    resource_coordinator::mojom::InterventionPolicy old_policy,
    resource_coordinator::mojom::InterventionPolicy new_policy) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  const size_t kIndex = ToIndex(intervention);

  // Invalidate the local policy aggregation for this intervention. It will be
  // recomputed on the next query to GetInterventionPolicy.
  intervention_policy_[kIndex] =
      resource_coordinator::mojom::InterventionPolicy::kUnknown;

  // The first time a frame transitions away from kUnknown for the last policy,
  // then that frame is considered to have checked in. Frames always provide
  // initial policy values in order, ensuring this works.
  if (old_policy == resource_coordinator::mojom::InterventionPolicy::kUnknown &&
      new_policy != resource_coordinator::mojom::InterventionPolicy::kUnknown &&
      intervention == resource_coordinator::mojom::
                          PolicyControlledIntervention::kMaxValue) {
    ++intervention_policy_frames_reported_;
    DCHECK_LE(intervention_policy_frames_reported_, frame_node_count_);
  }
}

resource_coordinator::mojom::InterventionPolicy
PageNodeImpl::GetInterventionPolicy(
    resource_coordinator::mojom::PolicyControlledIntervention intervention) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  // If there are no frames, or they've not all reported, then return kUnknown.
  if (frame_node_count_ == 0u ||
      intervention_policy_frames_reported_ != frame_node_count_) {
    return resource_coordinator::mojom::InterventionPolicy::kUnknown;
  }

  // Recompute the policy if it is currently invalid.
  const size_t kIndex = ToIndex(intervention);
  DCHECK_LE(kIndex, kMaxInterventionIndex);
  if (intervention_policy_[kIndex] ==
      resource_coordinator::mojom::InterventionPolicy::kUnknown) {
    RecomputeInterventionPolicy(intervention);
    DCHECK_NE(resource_coordinator::mojom::InterventionPolicy::kUnknown,
              intervention_policy_[kIndex]);
  }

  return intervention_policy_[kIndex];
}

void PageNodeImpl::JoinGraph() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  InvalidateAllInterventionPolicies();

  NodeBase::JoinGraph();
}

void PageNodeImpl::LeaveGraph() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  DCHECK_EQ(0u, frame_node_count_);

  NodeBase::LeaveGraph();
}

bool PageNodeImpl::IsPageAlmostIdle() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return page_almost_idle();
}

bool PageNodeImpl::IsVisible() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return is_visible();
}

base::TimeDelta PageNodeImpl::GetTimeSinceLastVisibilityChange() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return TimeSinceLastVisibilityChange();
}

bool PageNodeImpl::IsAudible() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return is_audible();
}

bool PageNodeImpl::IsLoading() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return is_loading();
}

ukm::SourceId PageNodeImpl::GetUkmSourceID() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return ukm_source_id();
}

PageNodeImpl::LifecycleState PageNodeImpl::GetLifecycleState() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return lifecycle_state();
}

int64_t PageNodeImpl::GetNavigationID() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return navigation_id();
}

base::TimeDelta PageNodeImpl::GetTimeSinceLastNavigation() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return TimeSinceLastNavigation();
}

const FrameNode* PageNodeImpl::GetMainFrameNode() const {
  return GetMainFrameNodeImpl();
}

const WebContentsProxy& PageNodeImpl::GetContentProxy() const {
  return contents_proxy();
}

const GURL& PageNodeImpl::GetMainFrameUrl() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return main_frame_url();
}

void PageNodeImpl::SetPageAlmostIdle(bool page_almost_idle) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  page_almost_idle_.SetAndMaybeNotify(this, page_almost_idle);
}

void PageNodeImpl::SetLifecycleState(LifecycleState lifecycle_state) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  lifecycle_state_.SetAndMaybeNotify(this, lifecycle_state);
}

void PageNodeImpl::InvalidateAllInterventionPolicies() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  for (size_t i = 0; i <= kMaxInterventionIndex; ++i)
    intervention_policy_[i] =
        resource_coordinator::mojom::InterventionPolicy::kUnknown;
}

// TODO(chrisha): Move this all out to a decorator.
void PageNodeImpl::MaybeInvalidateInterventionPolicies(
    FrameNodeImpl* frame_node,
    bool adding_frame) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  // Ensure that the frame was already added or removed as expected.
  DCHECK(!adding_frame || GraphImplOperations::HasFrame(this, frame_node));

  // Determine whether or not the frames had all reported prior to this change.
  const size_t prior_frame_count = frame_node_count_ + (adding_frame ? -1 : 1);
  const bool frames_all_reported_prior =
      prior_frame_count > 0 &&
      intervention_policy_frames_reported_ == prior_frame_count;

  // If the previous state was considered fully reported, then aggregation may
  // have occurred. Adding or removing a frame (even one that is fully reported)
  // needs to invalidate that aggregation. Invalidation could happen on every
  // single frame addition and removal, but only doing this when the previous
  // state was fully reported reduces unnecessary invalidations.
  if (frames_all_reported_prior)
    InvalidateAllInterventionPolicies();

  // Update the reporting frame count.
  const bool frame_reported = frame_node->AreAllInterventionPoliciesSet();
  if (frame_reported)
    intervention_policy_frames_reported_ += adding_frame ? 1 : -1;

  DCHECK_LE(intervention_policy_frames_reported_, frame_node_count_);
}

void PageNodeImpl::RecomputeInterventionPolicy(
    resource_coordinator::mojom::PolicyControlledIntervention intervention) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  const size_t kIndex = ToIndex(intervention);

  // This should never be called with an empty frame tree.
  DCHECK_NE(0u, frame_node_count_);

  resource_coordinator::mojom::InterventionPolicy policy =
      resource_coordinator::mojom::InterventionPolicy::kDefault;
  GraphImplOperations::VisitFrameTreePreOrder(
      this, [&policy, &kIndex](FrameNodeImpl* frame) -> bool {
        // No frame should have an unknown policy, as aggregation should only be
        // invoked after all frames have checked in.
        DCHECK_NE(resource_coordinator::mojom::InterventionPolicy::kUnknown,
                  frame->intervention_policy_[kIndex]);

        // If any frame opts out then the whole frame tree opts out, even if
        // other frames have opted in.
        if (frame->intervention_policy_[kIndex] ==
            resource_coordinator::mojom::InterventionPolicy::kOptOut) {
          policy = resource_coordinator::mojom::InterventionPolicy::kOptOut;
          return false;
        }

        // If any frame opts in and none opt out, then the whole tree opts in.
        if (frame->intervention_policy_[kIndex] ==
            resource_coordinator::mojom::InterventionPolicy::kOptIn) {
          policy = resource_coordinator::mojom::InterventionPolicy::kOptIn;
        }
        return true;
      });

  intervention_policy_[kIndex] = policy;
}

}  // namespace performance_manager
