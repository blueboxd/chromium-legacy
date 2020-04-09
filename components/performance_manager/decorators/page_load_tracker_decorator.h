// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PERFORMANCE_MANAGER_DECORATORS_PAGE_LOAD_TRACKER_DECORATOR_H_
#define COMPONENTS_PERFORMANCE_MANAGER_DECORATORS_PAGE_LOAD_TRACKER_DECORATOR_H_

#include "base/time/time.h"
#include "base/timer/timer.h"
#include "components/performance_manager/public/graph/frame_node.h"
#include "components/performance_manager/public/graph/graph.h"
#include "components/performance_manager/public/graph/node_data_describer.h"
#include "components/performance_manager/public/graph/page_node.h"
#include "components/performance_manager/public/graph/process_node.h"

namespace performance_manager {

class FrameNodeImpl;
class PageNodeImpl;
class ProcessNodeImpl;

// The PageLoadTracker decorator is responsible for determining when a page is
// loading. A page starts loading when incoming data starts arriving for a
// top-level load to a different document. It stops loading when it reaches an
// "almost idle" state, based on CPU and network quiescence, or after an
// absolute timeout. This state is then updated on PageNodes in a graph.
class PageLoadTrackerDecorator : public FrameNode::ObserverDefaultImpl,
                                 public GraphOwnedDefaultImpl,
                                 public NodeDataDescriberDefaultImpl,
                                 public ProcessNode::ObserverDefaultImpl {
 public:
  class Data;

  PageLoadTrackerDecorator();
  ~PageLoadTrackerDecorator() override;

  // FrameNodeObserver implementation:
  void OnNetworkAlmostIdleChanged(const FrameNode* frame_node) override;

  // GraphOwned implementation:
  void OnPassedToGraph(Graph* graph) override;
  void OnTakenFromGraph(Graph* graph) override;

  // NodeDataDescriber implementation:
  base::Value DescribePageNodeData(const PageNode* node) const override;

  // ProcessNodeObserver implementation:
  void OnMainThreadTaskLoadIsLow(const ProcessNode* process_node) override;

  // Invoked by PageLoadTrackerDecoratorHelper when corresponding
  // WebContentsObserver methods are invoked, and the WebContents is loading to
  // a different document.
  static void DidReceiveResponse(PageNodeImpl* page_node);
  static void DidStopLoading(PageNodeImpl* page_node);

 protected:
  friend class PageLoadTrackerDecoratorTest;

  // The amount of time a page has to be idle post-loading in order for it to be
  // considered loaded and idle. This is used in UpdateLoadIdleState
  // transitions.
  static constexpr base::TimeDelta kLoadedAndIdlingTimeout =
      base::TimeDelta::FromSeconds(1);

  // The maximum amount of time post-DidStopLoading a page can be waiting for
  // an idle state to occur before the page is simply considered loaded anyways.
  // Since PageAlmostIdle is intended as an "initial loading complete" signal,
  // it needs to eventually terminate. This is strictly greater than the
  // kLoadedAndIdlingTimeout.
  //
  // This is taken as the 95th percentile of tab loading times on desktop
  // (see SessionRestore.ForegroundTabFirstLoaded). This ensures that all tabs
  // eventually transition to loaded, even if they keep the main task queue
  // busy, or continue loading content.
  static constexpr base::TimeDelta kWaitingForIdleTimeout =
      base::TimeDelta::FromMinutes(1);

  // (Un)registers the various node observer flavors of this object with the
  // graph. These are invoked by OnPassedToGraph and OnTakenFromGraph, but
  // hoisted to their own functions for testing.
  void RegisterObservers(Graph* graph);
  void UnregisterObservers(Graph* graph);

  // These are called when properties/events affecting the load-idle state are
  // observed. Frame and Process variants will eventually all redirect to the
  // appropriate Page variant, where the real work is done.
  void UpdateLoadIdleStateFrame(FrameNodeImpl* frame_node);
  void UpdateLoadIdleStateProcess(ProcessNodeImpl* process_node);
  static void UpdateLoadIdleStatePage(PageNodeImpl* page_node);

  // Helper function for transitioning to the final state.
  static void TransitionToLoadedAndIdle(PageNodeImpl* page_node);

  static bool IsIdling(const PageNodeImpl* page_node);

 private:
  DISALLOW_COPY_AND_ASSIGN(PageLoadTrackerDecorator);
};

class PageLoadTrackerDecorator::Data {
 public:
  // The state transitions associated with a load. In general a page transitions
  // through these states from top to bottom.
  enum class LoadIdleState {
    // The initial state. Can only transition to kLoading from here.
    kLoadingNotStarted,
    // Incoming data has started to arrive for a load. Almost idle signals are
    // ignored in this state. Can transition to kLoadedNotIdling and
    // kLoadedAndIdling from here.
    kLoading,
    // Loading has completed, but the page has not started idling. Can only
    // transition to kLoadedAndIdling from here.
    kLoadedNotIdling,
    // Loading has completed, and the page is idling. Can transition to
    // kLoadedNotIdling or kLoadedAndIdle from here.
    kLoadedAndIdling,
    // Loading has completed and the page has been idling for sufficiently long.
    // This is the final state. Once this state has been reached a signal will
    // be emitted and no further state transitions will be tracked. Committing a
    // new non-same document navigation can start the cycle over again.
    kLoadedAndIdle
  };

  static Data* GetOrCreateForTesting(PageNodeImpl* page_node);
  static Data* GetForTesting(PageNodeImpl* page_node);
  static bool DestroyForTesting(PageNodeImpl* page_node);

  // Sets the LoadIdleState for the page, and updates PageNode::IsLoading()
  // accordingly.
  void SetLoadIdleState(PageNodeImpl* page_node, LoadIdleState load_idle_state);

  // Returns the LoadIdleState for the page.
  LoadIdleState load_idle_state() const { return load_idle_state_; }

  // Whether there is an ongoing different-document load for which data started
  // arriving.
  bool loading_received_response_ = false;

  // Marks the point in time when the DidStopLoading signal was received,
  // transitioning to kLoadedAndNotIdling or kLoadedAndIdling. This is used as
  // the basis for the kWaitingForIdleTimeout.
  base::TimeTicks loading_stopped_;

  // Marks the point in time when the last transition to kLoadedAndIdling
  // occurred. Used for gating the transition to kLoadedAndIdle.
  base::TimeTicks idling_started_;

  // A one-shot timer used for transitioning between kLoadedAndIdling and
  // kLoadedAndIdle.
  base::OneShotTimer idling_timer_;

 private:
  // Initially at kLoadingNotStarted. Transitions through the states via calls
  // to UpdateLoadIdleState. Is reset to kLoadingNotStarted when a non-same
  // document navigation is committed.
  LoadIdleState load_idle_state_ = LoadIdleState::kLoadingNotStarted;
};

}  // namespace performance_manager

#endif  // COMPONENTS_BROWSER_PERFORMANCE_MANAGER_DECORATORS_PAGE_LOAD_TRACKER_DECORATOR_H_
