// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/performance_manager/decorators/page_load_tracker_decorator.h"

#include <memory>
#include <type_traits>
#include <utility>

#include "base/memory/ptr_util.h"
#include "base/time/time.h"
#include "components/performance_manager/graph/frame_node_impl.h"
#include "components/performance_manager/graph/page_node_impl.h"
#include "components/performance_manager/graph/process_node_impl.h"
#include "components/performance_manager/test_support/graph_test_harness.h"
#include "components/performance_manager/test_support/mock_graphs.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace performance_manager {

// Aliasing these here makes this unittest much more legible.
using Data = PageLoadTrackerDecorator::Data;
using LIS = Data::LoadIdleState;

class PageLoadTrackerDecoratorTest : public GraphTestHarness {
 protected:
  PageLoadTrackerDecoratorTest() = default;
  ~PageLoadTrackerDecoratorTest() override = default;

  void SetUp() override {
    pltd_ = new PageLoadTrackerDecorator();
    graph()->PassToGraph(base::WrapUnique(pltd_));
  }

  void TestPageAlmostIdleTransitions(bool timeout);

  bool IsIdling(const PageNodeImpl* page_node) const {
    return PageLoadTrackerDecorator::IsIdling(page_node);
  }

  PageLoadTrackerDecorator* pltd_ = nullptr;

 private:
  DISALLOW_COPY_AND_ASSIGN(PageLoadTrackerDecoratorTest);
};

void PageLoadTrackerDecoratorTest::TestPageAlmostIdleTransitions(bool timeout) {
  static const base::TimeDelta kLoadedAndIdlingTimeout =
      PageLoadTrackerDecorator::kLoadedAndIdlingTimeout;
  static const base::TimeDelta kWaitingForIdleTimeout =
      PageLoadTrackerDecorator::kWaitingForIdleTimeout;

  MockSinglePageInSingleProcessGraph mock_graph(graph());
  auto* frame_node = mock_graph.frame.get();
  auto* page_node = mock_graph.page.get();
  auto* proc_node = mock_graph.process.get();
  auto* page_data = Data::GetOrCreateForTesting(page_node);

  // Initially the page should be in a loading not started state.
  EXPECT_EQ(LIS::kLoadingNotStarted, page_data->load_idle_state());
  EXPECT_FALSE(page_node->is_loading());
  EXPECT_FALSE(page_data->idling_timer_.IsRunning());

  // The state should transition to loading when DidReceiveResponse() is called
  // to indicate that loading starts.
  PageLoadTrackerDecorator::DidReceiveResponse(page_node);
  EXPECT_EQ(LIS::kLoading, page_data->load_idle_state());
  EXPECT_TRUE(page_node->is_loading());
  EXPECT_FALSE(page_data->idling_timer_.IsRunning());

  // Mark the page as idling. It should transition from kLoading directly
  // to kLoadedAndIdling after this.
  frame_node->SetNetworkAlmostIdle();
  proc_node->SetMainThreadTaskLoadIsLow(true);
  PageLoadTrackerDecorator::DidStopLoading(page_node);
  EXPECT_EQ(LIS::kLoadedAndIdling, page_data->load_idle_state());
  EXPECT_TRUE(page_node->is_loading());
  EXPECT_TRUE(page_data->idling_timer_.IsRunning());

  // Indicate that a response was received again. This should be ignored.
  PageLoadTrackerDecorator::DidReceiveResponse(page_node);
  EXPECT_EQ(LIS::kLoadedAndIdling, page_data->load_idle_state());
  EXPECT_TRUE(page_node->is_loading());
  EXPECT_TRUE(page_data->idling_timer_.IsRunning());
  PageLoadTrackerDecorator::DidStopLoading(page_node);
  EXPECT_EQ(LIS::kLoadedAndIdling, page_data->load_idle_state());
  EXPECT_TRUE(page_node->is_loading());
  EXPECT_TRUE(page_data->idling_timer_.IsRunning());

  // Go back to not idling. We should transition back to kLoadedNotIdling, and
  // a timer should still be running.
  frame_node->OnNavigationCommitted(GURL(), false);
  EXPECT_FALSE(frame_node->network_almost_idle());
  EXPECT_EQ(LIS::kLoadedNotIdling, page_data->load_idle_state());
  EXPECT_TRUE(page_data->idling_timer_.IsRunning());

  if (timeout) {
    // Let the timeout run down. The final state transition should occur.
    task_env().FastForwardBy(kWaitingForIdleTimeout);
    EXPECT_FALSE(Data::GetForTesting(page_node));
    EXPECT_FALSE(page_node->is_loading());
  } else {
    // Go back to idling.
    frame_node->SetNetworkAlmostIdle();
    EXPECT_TRUE(frame_node->network_almost_idle());
    EXPECT_EQ(LIS::kLoadedAndIdling, page_data->load_idle_state());
    EXPECT_TRUE(page_node->is_loading());
    EXPECT_TRUE(page_data->idling_timer_.IsRunning());

    // Let the idle timer evaluate. The final state transition should occur.
    task_env().FastForwardBy(kLoadedAndIdlingTimeout);
    EXPECT_FALSE(Data::GetForTesting(page_node));
    EXPECT_FALSE(page_node->is_loading());
  }

  // Firing other signals should not change the state at all.
  proc_node->SetMainThreadTaskLoadIsLow(false);
  EXPECT_FALSE(Data::GetForTesting(page_node));
  EXPECT_FALSE(page_node->is_loading());
  frame_node->OnNavigationCommitted(GURL(), false);
  EXPECT_FALSE(frame_node->network_almost_idle());
  EXPECT_FALSE(Data::GetForTesting(page_node));
  EXPECT_FALSE(page_node->is_loading());
}

TEST_F(PageLoadTrackerDecoratorTest, TestTransitionsNoTimeout) {
  TestPageAlmostIdleTransitions(false);
}

TEST_F(PageLoadTrackerDecoratorTest, TestTransitionsWithTimeout) {
  TestPageAlmostIdleTransitions(true);
}

TEST_F(PageLoadTrackerDecoratorTest, TestTransitionsNotIdlingOnDidStopLoading) {
  MockSinglePageInSingleProcessGraph mock_graph(graph());
  auto* frame_node = mock_graph.frame.get();
  auto* page_node = mock_graph.page.get();
  auto* proc_node = mock_graph.process.get();
  auto* page_data = Data::GetOrCreateForTesting(page_node);

  // Initially the page should be in a loading not started state.
  EXPECT_EQ(LIS::kLoadingNotStarted, page_data->load_idle_state());
  EXPECT_FALSE(page_node->is_loading());
  EXPECT_FALSE(page_data->idling_timer_.IsRunning());

  // The state should transition to loading when DidReceiveResponse() is called
  // to indicate that loading starts.
  PageLoadTrackerDecorator::DidReceiveResponse(page_node);
  EXPECT_EQ(LIS::kLoading, page_data->load_idle_state());
  EXPECT_TRUE(page_node->is_loading());
  EXPECT_FALSE(page_data->idling_timer_.IsRunning());

  // Mark the page as not idling.
  frame_node->OnNavigationCommitted(GURL(), false);
  proc_node->SetMainThreadTaskLoadIsLow(false);
  EXPECT_FALSE(IsIdling(page_node));

  // DidStopLoading() should cause a transition to kLoadedNotIdling.
  PageLoadTrackerDecorator::DidStopLoading(page_node);
  EXPECT_EQ(LIS::kLoadedNotIdling, page_data->load_idle_state());
  EXPECT_TRUE(page_node->is_loading());
  EXPECT_TRUE(page_data->idling_timer_.IsRunning());
}

TEST_F(PageLoadTrackerDecoratorTest, IsIdling) {
  MockSinglePageInSingleProcessGraph mock_graph(graph());
  auto* frame_node = mock_graph.frame.get();
  auto* page_node = mock_graph.page.get();
  auto* proc_node = mock_graph.process.get();

  // Neither of the idling properties are set, so IsIdling should return false.
  EXPECT_FALSE(IsIdling(page_node));

  // Should still return false after main thread task is low.
  proc_node->SetMainThreadTaskLoadIsLow(true);
  EXPECT_FALSE(IsIdling(page_node));

  // Should return true when network is idle.
  frame_node->SetNetworkAlmostIdle();
  EXPECT_TRUE(IsIdling(page_node));

  // Should toggle with main thread task low.
  proc_node->SetMainThreadTaskLoadIsLow(false);
  EXPECT_FALSE(IsIdling(page_node));
  proc_node->SetMainThreadTaskLoadIsLow(true);
  EXPECT_TRUE(IsIdling(page_node));

  // Should return false when network is no longer idle.
  frame_node->OnNavigationCommitted(GURL(), false);
  EXPECT_FALSE(IsIdling(page_node));

  // And should stay false if main thread task also goes low again.
  proc_node->SetMainThreadTaskLoadIsLow(false);
  EXPECT_FALSE(IsIdling(page_node));
}

}  // namespace performance_manager
