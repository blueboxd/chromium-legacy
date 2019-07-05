// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PERFORMANCE_MANAGER_GRAPH_PAGE_NODE_IMPL_H_
#define CHROME_BROWSER_PERFORMANCE_MANAGER_GRAPH_PAGE_NODE_IMPL_H_

#include <memory>
#include <vector>

#include "base/containers/flat_set.h"
#include "base/macros.h"
#include "base/time/time.h"
#include "chrome/browser/performance_manager/graph/node_attached_data.h"
#include "chrome/browser/performance_manager/graph/node_base.h"
#include "chrome/browser/performance_manager/observers/graph_observer.h"
#include "chrome/browser/performance_manager/public/graph/page_node.h"
#include "chrome/browser/performance_manager/public/web_contents_proxy.h"
#include "url/gurl.h"

namespace performance_manager {

class FrameNodeImpl;

class PageNodeImpl : public PublicNodeImpl<PageNodeImpl, PageNode>,
                     public TypedNodeBase<PageNodeImpl,
                                          GraphImplObserver,
                                          PageNode,
                                          PageNodeObserver> {
 public:
  static constexpr NodeTypeEnum Type() { return NodeTypeEnum::kPage; }

  PageNodeImpl(GraphImpl* graph,
               const WebContentsProxy& contents_proxy,
               bool is_visible,
               bool is_audible);
  ~PageNodeImpl() override;

  // Returns the web contents associated with this page node. It is valid to
  // call this function on any thread but the weak pointer must only be
  // dereferenced on the UI thread.
  const WebContentsProxy& contents_proxy() const;

  void SetIsLoading(bool is_loading);
  void SetIsVisible(bool is_visible);
  void SetIsAudible(bool is_audible);
  void SetUkmSourceId(ukm::SourceId ukm_source_id);
  void OnFaviconUpdated();
  void OnTitleUpdated();
  void OnMainFrameNavigationCommitted(base::TimeTicks navigation_committed_time,
                                      int64_t navigation_id,
                                      const GURL& url);

  // Returns the average CPU usage that can be attributed to this page over the
  // last measurement period. CPU usage is expressed as the average percentage
  // of cores occupied over the last measurement interval. One core fully
  // occupied would be 100, while two cores at 5% each would be 10.
  // TODO(chrisha): Make this 1.0 for 100%, and 0.1 for 10%.
  double GetCPUUsage() const;

  // Returns 0 if no navigation has happened, otherwise returns the time since
  // the last navigation commit.
  base::TimeDelta TimeSinceLastNavigation() const;

  // Returns the time since the last visibility change, it should always have a
  // value since we set the visibility property when we create a
  // page node.
  base::TimeDelta TimeSinceLastVisibilityChange() const;

  // Returns the current main frame node (if there is one), otherwise returns
  // any of the potentially multiple main frames that currently exist. If there
  // are no main frames at the moment, returns nullptr.
  FrameNodeImpl* GetMainFrameNodeImpl() const;

  // Accessors.
  bool is_visible() const;
  bool is_audible() const;
  bool is_loading() const;
  ukm::SourceId ukm_source_id() const;
  LifecycleState lifecycle_state() const;
  const base::flat_set<FrameNodeImpl*>& main_frame_nodes() const;
  base::TimeTicks usage_estimate_time() const;
  base::TimeDelta cumulative_cpu_usage_estimate() const;
  uint64_t private_footprint_kb_estimate() const;
  bool page_almost_idle() const;
  const GURL& main_frame_url() const;
  int64_t navigation_id() const;

  void set_usage_estimate_time(base::TimeTicks usage_estimate_time);
  void set_cumulative_cpu_usage_estimate(
      base::TimeDelta cumulative_cpu_usage_estimate);
  void set_private_footprint_kb_estimate(
      uint64_t private_footprint_kb_estimate);
  void set_has_nonempty_beforeunload(bool has_nonempty_beforeunload);

  // Invoked when a frame belonging to this page changes intervention policy
  // values.
  // TODO(chrisha): Move this out to a decorator.
  void OnFrameInterventionPolicyChanged(
      FrameNodeImpl* frame,
      resource_coordinator::mojom::PolicyControlledIntervention intervention,
      resource_coordinator::mojom::InterventionPolicy old_policy,
      resource_coordinator::mojom::InterventionPolicy new_policy);

  // Gets the current policy for the specified |intervention|, recomputing it
  // from individual frame policies if necessary. Returns kUnknown until there
  // are 1 or more frames, and they have all computed their local policy
  // settings.
  resource_coordinator::mojom::InterventionPolicy GetInterventionPolicy(
      resource_coordinator::mojom::PolicyControlledIntervention intervention);

  // Similar to GetInterventionPolicy, but doesn't trigger recomputes.
  resource_coordinator::mojom::InterventionPolicy
  GetRawInterventionPolicyForTesting(
      resource_coordinator::mojom::PolicyControlledIntervention intervention)
      const {
    return intervention_policy_[static_cast<size_t>(intervention)];
  }

  size_t GetInterventionPolicyFramesReportedForTesting() const {
    return intervention_policy_frames_reported_;
  }

  void SetLifecycleStateForTesting(LifecycleState lifecycle_state) {
    SetLifecycleState(lifecycle_state);
  }

  void SetPageAlmostIdleForTesting(bool page_almost_idle) {
    SetPageAlmostIdle(page_almost_idle);
  }

 private:
  friend class FrameNodeImpl;
  friend class FrozenFrameAggregatorAccess;
  friend class PageAlmostIdleAccess;

  // PageNode implementation:
  bool IsPageAlmostIdle() const override;
  bool IsVisible() const override;
  base::TimeDelta GetTimeSinceLastVisibilityChange() const override;
  bool IsAudible() const override;
  bool IsLoading() const override;
  ukm::SourceId GetUkmSourceID() const override;
  LifecycleState GetLifecycleState() const override;
  int64_t GetNavigationID() const override;
  base::TimeDelta GetTimeSinceLastNavigation() const override;
  const FrameNode* GetMainFrameNode() const override;
  const GURL& GetMainFrameUrl() const override;
  const WebContentsProxy& GetContentProxy() const override;

  void AddFrame(FrameNodeImpl* frame_node);
  void RemoveFrame(FrameNodeImpl* frame_node);
  void JoinGraph() override;
  void LeaveGraph() override;

  void SetPageAlmostIdle(bool page_almost_idle);
  void SetLifecycleState(LifecycleState lifecycle_state);

  // Invalidates all currently aggregated intervention policies.
  void InvalidateAllInterventionPolicies();

  // Invoked when adding or removing a frame. This will update
  // |intervention_policy_frames_reported_| if necessary and potentially
  // invalidate the aggregated intervention policies. This should be called
  // after the frame has already been added or removed from
  // |frame_nodes_|.
  void MaybeInvalidateInterventionPolicies(FrameNodeImpl* frame_node,
                                           bool adding_frame);

  // Recomputes intervention policy aggregation. This is invoked on demand when
  // a policy is queried.
  void RecomputeInterventionPolicy(
      resource_coordinator::mojom::PolicyControlledIntervention intervention);

  // The WebContentsProxy associated with this page.
  const WebContentsProxy contents_proxy_;

  // The main frame nodes of this page. There can be more than one main frame
  // in a page, among other reasons because during main frame navigation, the
  // pending navigation will coexist with the existing main frame until it's
  // committed.
  base::flat_set<FrameNodeImpl*> main_frame_nodes_;

  // The total count of frames that tally up to this page.
  size_t frame_node_count_ = 0;

  // The last time at which the page visibility changed.
  base::TimeTicks visibility_change_time_;

  // The last time at which a main frame navigation was committed.
  base::TimeTicks navigation_committed_time_;

  // The time the most recent resource usage estimate applies to.
  base::TimeTicks usage_estimate_time_;

  // The most current CPU usage estimate. Note that this estimate is most
  // generously described as "piecewise linear", as it attributes the CPU
  // cost incurred since the last measurement was made equally to pages
  // hosted by a process. If, e.g. a frame has come into existence and vanished
  // from a given process between measurements, the entire cost to that frame
  // will be mis-attributed to other frames hosted in that process.
  base::TimeDelta cumulative_cpu_usage_estimate_;

  // The most current memory footprint estimate.
  uint64_t private_footprint_kb_estimate_ = 0;

  // Indicates whether or not this page has a non-empty beforeunload handler.
  // This is an aggregation of the same value on each frame in the page's frame
  // tree. The aggregation is made at the moment all frames associated with a
  // page have transition to frozen.
  bool has_nonempty_beforeunload_ = false;

  // The URL the main frame last committed a navigation to and the unique ID of
  // the associated navigation handle.
  GURL main_frame_url_;
  int64_t navigation_id_ = 0;

  // The aggregate intervention policy states for this page. These are
  // aggregated from the corresponding per-frame values. If an individual value
  // is kUnknown then a frame in the frame tree has changed values and
  // a new aggregation is required.
  resource_coordinator::mojom::InterventionPolicy
      intervention_policy_[static_cast<size_t>(
                               resource_coordinator::mojom::
                                   PolicyControlledIntervention::kMaxValue) +
                           1];

  // The number of child frames that have checked in with initial intervention
  // policy values. If this doesn't match the number of known child frames, then
  // aggregation isn't possible. Child frames check in with all properties once
  // immediately after document parsing, and the *last* value being set
  // is used as a signal that the frame has reported.
  size_t intervention_policy_frames_reported_ = 0;

  // Page almost idle state. This is the output that is driven by the
  // PageAlmostIdleDecorator.
  ObservedProperty::NotifiesOnlyOnChanges<
      bool,
      &GraphImplObserver::OnPageAlmostIdleChanged,
      &PageNodeObserver::OnPageAlmostIdleChanged>
      page_almost_idle_{false};
  // Whether or not the page is visible. Driven by browser instrumentation.
  // Initialized on construction.
  ObservedProperty::NotifiesOnlyOnChanges<
      bool,
      &GraphImplObserver::OnIsVisibleChanged,
      &PageNodeObserver::OnIsVisibleChanged>
      is_visible_{false};
  // Whether or not the page is audible. Driven by browser instrumentation.
  // Initialized on construction.
  ObservedProperty::NotifiesOnlyOnChanges<
      bool,
      &GraphImplObserver::OnIsAudibleChanged,
      &PageNodeObserver::OnIsAudibleChanged>
      is_audible_{false};
  // The loading state. This is driven by instrumentation in the browser
  // process.
  ObservedProperty::NotifiesOnlyOnChanges<
      bool,
      &GraphImplObserver::OnIsLoadingChanged,
      &PageNodeObserver::OnIsLoadingChanged>
      is_loading_{false};
  // The UKM source ID associated with the URL of the main frame of this page.
  ObservedProperty::NotifiesOnlyOnChanges<
      ukm::SourceId,
      &GraphImplObserver::OnUkmSourceIdChanged,
      &PageNodeObserver::OnUkmSourceIdChanged>
      ukm_source_id_{ukm::kInvalidSourceId};
  // The lifecycle state of this page. This is aggregated from the lifecycle
  // state of each frame in the frame tree.
  ObservedProperty::NotifiesOnlyOnChanges<
      LifecycleState,
      &GraphImplObserver::OnLifecycleStateChanged,
      &PageNodeObserver::OnPageLifecycleStateChanged>
      lifecycle_state_{LifecycleState::kRunning};

  // Storage for PageAlmostIdle user data.
  std::unique_ptr<NodeAttachedData> page_almost_idle_data_;

  // Inline storage for FrozenFrameAggregator user data.
  InternalNodeAttachedDataStorage<sizeof(uintptr_t) + 8> frozen_frame_data_;

  DISALLOW_COPY_AND_ASSIGN(PageNodeImpl);
};

}  // namespace performance_manager

#endif  // CHROME_BROWSER_PERFORMANCE_MANAGER_GRAPH_PAGE_NODE_IMPL_H_
