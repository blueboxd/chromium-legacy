// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/performance_manager/graph/page_node_impl.h"

#include "base/stl_util.h"
#include "base/test/simple_test_tick_clock.h"
#include "chrome/browser/performance_manager/graph/frame_node_impl.h"
#include "chrome/browser/performance_manager/graph/graph_impl_operations.h"
#include "chrome/browser/performance_manager/graph/graph_test_harness.h"
#include "chrome/browser/performance_manager/graph/mock_graphs.h"
#include "chrome/browser/performance_manager/graph/page_node_impl.h"
#include "chrome/browser/performance_manager/graph/process_node_impl.h"
#include "chrome/browser/performance_manager/performance_manager_clock.h"
#include "chrome/browser/performance_manager/public/graph/page_node.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace performance_manager {

namespace {

class PageNodeImplTest : public GraphTestHarness {
 public:
  void SetUp() override {
    PerformanceManagerClock::SetClockForTesting(&clock_);

    // Sets a valid starting time.
    clock_.SetNowTicks(base::TimeTicks::Now());
  }

  void TearDown() override { PerformanceManagerClock::ResetClockForTesting(); }

 protected:
  void AdvanceClock(base::TimeDelta delta) { clock_.Advance(delta); }

 private:
  base::SimpleTestTickClock clock_;
};

}  // namespace

TEST_F(PageNodeImplTest, SafeDowncast) {
  auto page = CreateNode<PageNodeImpl>();
  PageNode* node = page.get();
  EXPECT_EQ(page.get(), PageNodeImpl::FromNode(node));
  NodeBase* base = page.get();
  EXPECT_EQ(base, NodeBase::FromNode(node));
  EXPECT_EQ(static_cast<Node*>(node), base->ToNode());
}

TEST_F(PageNodeImplTest, AddFrameBasic) {
  auto process_node = CreateNode<ProcessNodeImpl>();
  auto page_node = CreateNode<PageNodeImpl>();
  auto parent_frame =
      CreateNode<FrameNodeImpl>(process_node.get(), page_node.get());
  auto child1_frame = CreateNode<FrameNodeImpl>(
      process_node.get(), page_node.get(), parent_frame.get(), 1);
  auto child2_frame = CreateNode<FrameNodeImpl>(
      process_node.get(), page_node.get(), parent_frame.get(), 2);

  // Validate that all frames are tallied to the page.
  EXPECT_EQ(3u, GraphImplOperations::GetFrameNodes(page_node.get()).size());
}

TEST_F(PageNodeImplTest, RemoveFrame) {
  auto process_node = CreateNode<ProcessNodeImpl>();
  auto page_node = CreateNode<PageNodeImpl>();
  auto frame_node = CreateNode<FrameNodeImpl>(process_node.get(),
                                              page_node.get(), nullptr, 0);

  // Ensure correct page-frame relationship has been established.
  auto frame_nodes = GraphImplOperations::GetFrameNodes(page_node.get());
  EXPECT_EQ(1u, frame_nodes.size());
  EXPECT_TRUE(base::Contains(frame_nodes, frame_node.get()));
  EXPECT_EQ(page_node.get(), frame_node->page_node());

  frame_node.reset();

  // Parent-child relationships should no longer exist.
  EXPECT_EQ(0u, GraphImplOperations::GetFrameNodes(page_node.get()).size());
}

TEST_F(PageNodeImplTest, CalculatePageCPUUsageForSinglePageInSingleProcess) {
  MockSinglePageInSingleProcessGraph mock_graph(graph());
  mock_graph.process->SetCPUUsage(40);
  EXPECT_EQ(40, mock_graph.page->GetCPUUsage());
}

TEST_F(PageNodeImplTest, CalculatePageCPUUsageForMultiplePagesInSingleProcess) {
  MockMultiplePagesInSingleProcessGraph mock_graph(graph());
  mock_graph.process->SetCPUUsage(40);
  EXPECT_EQ(20, mock_graph.page->GetCPUUsage());
  EXPECT_EQ(20, mock_graph.other_page->GetCPUUsage());
}

TEST_F(PageNodeImplTest,
       CalculatePageCPUUsageForSinglePageWithMultipleProcesses) {
  MockSinglePageWithMultipleProcessesGraph mock_graph(graph());
  mock_graph.process->SetCPUUsage(40);
  mock_graph.other_process->SetCPUUsage(30);
  EXPECT_EQ(70, mock_graph.page->GetCPUUsage());
}

TEST_F(PageNodeImplTest,
       CalculatePageCPUUsageForMultiplePagesWithMultipleProcesses) {
  MockMultiplePagesWithMultipleProcessesGraph mock_graph(graph());
  mock_graph.process->SetCPUUsage(40);
  mock_graph.other_process->SetCPUUsage(30);
  EXPECT_EQ(20, mock_graph.page->GetCPUUsage());
  EXPECT_EQ(50, mock_graph.other_page->GetCPUUsage());
}

TEST_F(PageNodeImplTest, TimeSinceLastVisibilityChange) {
  MockSinglePageInSingleProcessGraph mock_graph(graph());

  mock_graph.page->SetIsVisible(true);
  EXPECT_TRUE(mock_graph.page->is_visible());
  AdvanceClock(base::TimeDelta::FromSeconds(42));
  EXPECT_EQ(base::TimeDelta::FromSeconds(42),
            mock_graph.page->TimeSinceLastVisibilityChange());

  mock_graph.page->SetIsVisible(false);
  AdvanceClock(base::TimeDelta::FromSeconds(23));
  EXPECT_EQ(base::TimeDelta::FromSeconds(23),
            mock_graph.page->TimeSinceLastVisibilityChange());
  EXPECT_FALSE(mock_graph.page->is_visible());
}

TEST_F(PageNodeImplTest, TimeSinceLastNavigation) {
  MockSinglePageInSingleProcessGraph mock_graph(graph());
  // Before any commit events, timedelta should be 0.
  EXPECT_TRUE(mock_graph.page->TimeSinceLastNavigation().is_zero());

  // 1st navigation.
  GURL url("http://www.example.org");
  mock_graph.page->OnMainFrameNavigationCommitted(
      PerformanceManagerClock::NowTicks(), 10u, url);
  EXPECT_EQ(url, mock_graph.page->main_frame_url());
  EXPECT_EQ(10u, mock_graph.page->navigation_id());
  AdvanceClock(base::TimeDelta::FromSeconds(11));
  EXPECT_EQ(base::TimeDelta::FromSeconds(11),
            mock_graph.page->TimeSinceLastNavigation());

  // 2nd navigation.
  url = GURL("http://www.example.org/bobcat");
  mock_graph.page->OnMainFrameNavigationCommitted(
      PerformanceManagerClock::NowTicks(), 20u, url);
  EXPECT_EQ(url, mock_graph.page->main_frame_url());
  EXPECT_EQ(20u, mock_graph.page->navigation_id());
  AdvanceClock(base::TimeDelta::FromSeconds(17));
  EXPECT_EQ(base::TimeDelta::FromSeconds(17),
            mock_graph.page->TimeSinceLastNavigation());
}

TEST_F(PageNodeImplTest, IsLoading) {
  MockSinglePageInSingleProcessGraph mock_graph(graph());
  auto* page_node = mock_graph.page.get();

  // This should be initialized to false.
  EXPECT_FALSE(page_node->is_loading());

  // Set to false and the property should stay false.
  page_node->SetIsLoading(false);
  EXPECT_FALSE(page_node->is_loading());

  // Set to true and the property should read true.
  page_node->SetIsLoading(true);
  EXPECT_TRUE(page_node->is_loading());

  // Set to false and the property should read false again.
  page_node->SetIsLoading(false);
  EXPECT_FALSE(page_node->is_loading());
}

namespace {

const size_t kInterventionCount =
    static_cast<size_t>(
        resource_coordinator::mojom::PolicyControlledIntervention::kMaxValue) +
    1;

void ExpectRawInterventionPolicy(
    resource_coordinator::mojom::InterventionPolicy policy,
    const PageNodeImpl* page_node) {
  for (size_t i = 0; i < kInterventionCount; ++i) {
    EXPECT_EQ(
        policy,
        page_node->GetRawInterventionPolicyForTesting(
            static_cast<
                resource_coordinator::mojom::PolicyControlledIntervention>(i)));
  }
}

void ExpectInterventionPolicy(
    resource_coordinator::mojom::InterventionPolicy policy,
    PageNodeImpl* page_node) {
  for (size_t i = 0; i < kInterventionCount; ++i) {
    EXPECT_EQ(
        policy,
        page_node->GetInterventionPolicy(
            static_cast<
                resource_coordinator::mojom::PolicyControlledIntervention>(i)));
  }
}

void ExpectInitialInterventionPolicyAggregationWorks(
    GraphImpl* mock_graph,
    resource_coordinator::mojom::InterventionPolicy f0_policy,
    resource_coordinator::mojom::InterventionPolicy f1_policy,
    resource_coordinator::mojom::InterventionPolicy f0_policy_aggregated,
    resource_coordinator::mojom::InterventionPolicy f0f1_policy_aggregated) {
  TestNodeWrapper<ProcessNodeImpl> process =
      TestNodeWrapper<ProcessNodeImpl>::Create(mock_graph);
  TestNodeWrapper<PageNodeImpl> page =
      TestNodeWrapper<PageNodeImpl>::Create(mock_graph);

  // Check the initial values before any frames are added.
  EXPECT_EQ(0u, page->GetInterventionPolicyFramesReportedForTesting());
  ExpectRawInterventionPolicy(
      resource_coordinator::mojom::InterventionPolicy::kUnknown, page.get());
  ExpectInterventionPolicy(
      resource_coordinator::mojom::InterventionPolicy::kUnknown, page.get());

  // Create an initial frame.
  TestNodeWrapper<FrameNodeImpl> f0 = TestNodeWrapper<FrameNodeImpl>::Create(
      mock_graph, process.get(), page.get());
  // Add a frame and expect the values to be invalidated. Reaggregate and
  // ensure the appropriate value results.
  f0->SetAllInterventionPoliciesForTesting(f0_policy);
  EXPECT_EQ(1u, page->GetInterventionPolicyFramesReportedForTesting());
  ExpectRawInterventionPolicy(
      resource_coordinator::mojom::InterventionPolicy::kUnknown, page.get());
  ExpectInterventionPolicy(f0_policy_aggregated, page.get());

  TestNodeWrapper<FrameNodeImpl> f1 = TestNodeWrapper<FrameNodeImpl>::Create(
      mock_graph, process.get(), page.get(), f0.get(), 1);
  // Do it again. This time the raw values should be the same as the
  // aggregated values above.
  f1->SetAllInterventionPoliciesForTesting(f1_policy);
  EXPECT_EQ(2u, page->GetInterventionPolicyFramesReportedForTesting());
  ExpectRawInterventionPolicy(
      resource_coordinator::mojom::InterventionPolicy::kUnknown, page.get());
  ExpectInterventionPolicy(f0f1_policy_aggregated, page.get());

  // Remove a frame and expect the values to be invalidated again.
  f1.reset();
  EXPECT_EQ(1u, page->GetInterventionPolicyFramesReportedForTesting());
  ExpectRawInterventionPolicy(
      resource_coordinator::mojom::InterventionPolicy::kUnknown, page.get());
  ExpectInterventionPolicy(f0_policy_aggregated, page.get());
}

}  // namespace

TEST_F(PageNodeImplTest, InitialInterventionPolicy) {
  auto* mock_graph = graph();

  // Tests all possible transitions where the frame node has its policy values
  // set before being attached to the page node. This affectively tests the
  // aggregation logic in isolation.

  // Default x [Default, OptIn, OptOut]

  ExpectInitialInterventionPolicyAggregationWorks(
      mock_graph,
      resource_coordinator::mojom::InterventionPolicy::kDefault /* f0_policy */,
      resource_coordinator::mojom::InterventionPolicy::kDefault /* f1_policy */,
      resource_coordinator::mojom::InterventionPolicy::
          kDefault /* f0_policy_aggregated */,
      resource_coordinator::mojom::InterventionPolicy::
          kDefault /* f0f1_policy_aggregated */);

  ExpectInitialInterventionPolicyAggregationWorks(
      mock_graph,
      resource_coordinator::mojom::InterventionPolicy::kDefault /* f0_policy */,
      resource_coordinator::mojom::InterventionPolicy::kOptIn /* f1_policy */,
      resource_coordinator::mojom::InterventionPolicy::
          kDefault /* f0_policy_aggregated */,
      resource_coordinator::mojom::InterventionPolicy::
          kOptIn /* f0f1_policy_aggregated */);

  ExpectInitialInterventionPolicyAggregationWorks(
      mock_graph,
      resource_coordinator::mojom::InterventionPolicy::kDefault /* f0_policy */,
      resource_coordinator::mojom::InterventionPolicy::kOptOut /* f1_policy */,
      resource_coordinator::mojom::InterventionPolicy::
          kDefault /* f0_policy_aggregated */,
      resource_coordinator::mojom::InterventionPolicy::
          kOptOut /* f0f1_policy_aggregated */);

  // OptIn x [Default, OptIn, OptOut]

  ExpectInitialInterventionPolicyAggregationWorks(
      mock_graph,
      resource_coordinator::mojom::InterventionPolicy::kOptIn /* f0_policy */,
      resource_coordinator::mojom::InterventionPolicy::kDefault /* f1_policy */,
      resource_coordinator::mojom::InterventionPolicy::
          kOptIn /* f0_policy_aggregated */,
      resource_coordinator::mojom::InterventionPolicy::
          kOptIn /* f0f1_policy_aggregated */);

  ExpectInitialInterventionPolicyAggregationWorks(
      mock_graph,
      resource_coordinator::mojom::InterventionPolicy::kOptIn /* f0_policy */,
      resource_coordinator::mojom::InterventionPolicy::kOptIn /* f1_policy */,
      resource_coordinator::mojom::InterventionPolicy::
          kOptIn /* f0_policy_aggregated */,
      resource_coordinator::mojom::InterventionPolicy::
          kOptIn /* f0f1_policy_aggregated */);

  ExpectInitialInterventionPolicyAggregationWorks(
      mock_graph,
      resource_coordinator::mojom::InterventionPolicy::kOptIn /* f0_policy */,
      resource_coordinator::mojom::InterventionPolicy::kOptOut /* f1_policy */,
      resource_coordinator::mojom::InterventionPolicy::
          kOptIn /* f0_policy_aggregated */,
      resource_coordinator::mojom::InterventionPolicy::
          kOptOut /* f0f1_policy_aggregated */);

  // OptOut x [Default, OptIn, OptOut]

  ExpectInitialInterventionPolicyAggregationWorks(
      mock_graph,
      resource_coordinator::mojom::InterventionPolicy::kOptOut /* f0_policy */,
      resource_coordinator::mojom::InterventionPolicy::kDefault /* f1_policy */,
      resource_coordinator::mojom::InterventionPolicy::
          kOptOut /* f0_policy_aggregated */,
      resource_coordinator::mojom::InterventionPolicy::
          kOptOut /* f0f1_policy_aggregated */);

  ExpectInitialInterventionPolicyAggregationWorks(
      mock_graph,
      resource_coordinator::mojom::InterventionPolicy::kOptOut /* f0_policy */,
      resource_coordinator::mojom::InterventionPolicy::kOptIn /* f1_policy */,
      resource_coordinator::mojom::InterventionPolicy::
          kOptOut /* f0_policy_aggregated */,
      resource_coordinator::mojom::InterventionPolicy::
          kOptOut /* f0f1_policy_aggregated */);

  ExpectInitialInterventionPolicyAggregationWorks(
      mock_graph,
      resource_coordinator::mojom::InterventionPolicy::kOptOut /* f0_policy */,
      resource_coordinator::mojom::InterventionPolicy::kOptOut /* f1_policy */,
      resource_coordinator::mojom::InterventionPolicy::
          kOptOut /* f0_policy_aggregated */,
      resource_coordinator::mojom::InterventionPolicy::
          kOptOut /* f0f1_policy_aggregated */);
}

TEST_F(PageNodeImplTest, IncrementalInterventionPolicy) {
  auto* mock_graph = graph();

  TestNodeWrapper<ProcessNodeImpl> process =
      TestNodeWrapper<ProcessNodeImpl>::Create(mock_graph);
  TestNodeWrapper<PageNodeImpl> page =
      TestNodeWrapper<PageNodeImpl>::Create(mock_graph);

  // Create two frames and immediately attach them to the page.
  TestNodeWrapper<FrameNodeImpl> f0 = TestNodeWrapper<FrameNodeImpl>::Create(
      mock_graph, process.get(), page.get());
  TestNodeWrapper<FrameNodeImpl> f1 = TestNodeWrapper<FrameNodeImpl>::Create(
      mock_graph, process.get(), page.get(), f0.get(), 1);
  EXPECT_EQ(0u, page->GetInterventionPolicyFramesReportedForTesting());
  EXPECT_EQ(0u, page->GetInterventionPolicyFramesReportedForTesting());
  EXPECT_EQ(0u, page->GetInterventionPolicyFramesReportedForTesting());

  // Set the policies on the first frame. This should be observed by the page
  // node, but aggregation should still not be possible.
  f0->SetAllInterventionPoliciesForTesting(
      resource_coordinator::mojom::InterventionPolicy::kDefault);
  EXPECT_EQ(1u, page->GetInterventionPolicyFramesReportedForTesting());
  ExpectRawInterventionPolicy(
      resource_coordinator::mojom::InterventionPolicy::kUnknown, page.get());
  ExpectInterventionPolicy(
      resource_coordinator::mojom::InterventionPolicy::kUnknown, page.get());

  // Now set the policy on the second frame. This should be observed and an
  // aggregated page policy value should now be available.
  f1->SetAllInterventionPoliciesForTesting(
      resource_coordinator::mojom::InterventionPolicy::kDefault);
  EXPECT_EQ(2u, page->GetInterventionPolicyFramesReportedForTesting());
  ExpectRawInterventionPolicy(
      resource_coordinator::mojom::InterventionPolicy::kUnknown, page.get());
  ExpectInterventionPolicy(
      resource_coordinator::mojom::InterventionPolicy::kDefault, page.get());

  // Change the policy value on a frame and expect a new aggregation to be
  // required.
  f1->SetAllInterventionPoliciesForTesting(
      resource_coordinator::mojom::InterventionPolicy::kOptIn);
  EXPECT_EQ(2u, page->GetInterventionPolicyFramesReportedForTesting());
  ExpectRawInterventionPolicy(
      resource_coordinator::mojom::InterventionPolicy::kUnknown, page.get());
  ExpectInterventionPolicy(
      resource_coordinator::mojom::InterventionPolicy::kOptIn, page.get());
}

namespace {

class LenientMockObserver : public PageNodeImpl::Observer {
 public:
  LenientMockObserver() {}
  ~LenientMockObserver() override {}

  MOCK_METHOD1(OnPageNodeAdded, void(const PageNode*));
  MOCK_METHOD1(OnBeforePageNodeRemoved, void(const PageNode*));
  MOCK_METHOD1(OnIsVisibleChanged, void(const PageNode*));
  MOCK_METHOD1(OnIsAudibleChanged, void(const PageNode*));
  MOCK_METHOD1(OnIsLoadingChanged, void(const PageNode*));
  MOCK_METHOD1(OnUkmSourceIdChanged, void(const PageNode*));
  MOCK_METHOD1(OnPageLifecycleStateChanged, void(const PageNode*));
  MOCK_METHOD1(OnPageAlmostIdleChanged, void(const PageNode*));
  MOCK_METHOD1(OnMainFrameNavigationCommitted, void(const PageNode*));
  MOCK_METHOD1(OnTitleUpdated, void(const PageNode*));
  MOCK_METHOD1(OnFaviconUpdated, void(const PageNode*));

  void SetNotifiedPageNode(const PageNode* page_node) {
    notified_page_node_ = page_node;
  }

  const PageNode* TakeNotifiedPageNode() {
    const PageNode* node = notified_page_node_;
    notified_page_node_ = nullptr;
    return node;
  }

 private:
  const PageNode* notified_page_node_ = nullptr;
};

using MockObserver = ::testing::StrictMock<LenientMockObserver>;

using testing::_;
using testing::Invoke;

}  // namespace

TEST_F(PageNodeImplTest, ObserverWorks) {
  auto process = CreateNode<ProcessNodeImpl>();

  MockObserver obs;
  graph()->AddPageNodeObserver(&obs);

  // Create a page node and expect a matching call to "OnPageNodeAdded".
  EXPECT_CALL(obs, OnPageNodeAdded(_))
      .WillOnce(Invoke(&obs, &MockObserver::SetNotifiedPageNode));
  auto page_node = CreateNode<PageNodeImpl>();
  const PageNode* raw_page_node = page_node.get();
  EXPECT_EQ(raw_page_node, obs.TakeNotifiedPageNode());

  EXPECT_CALL(obs, OnIsVisibleChanged(_))
      .WillOnce(Invoke(&obs, &MockObserver::SetNotifiedPageNode));
  page_node->SetIsVisible(true);
  EXPECT_EQ(raw_page_node, obs.TakeNotifiedPageNode());

  EXPECT_CALL(obs, OnIsAudibleChanged(_))
      .WillOnce(Invoke(&obs, &MockObserver::SetNotifiedPageNode));
  page_node->SetIsAudible(true);
  EXPECT_EQ(raw_page_node, obs.TakeNotifiedPageNode());

  EXPECT_CALL(obs, OnIsLoadingChanged(_))
      .WillOnce(Invoke(&obs, &MockObserver::SetNotifiedPageNode));
  page_node->SetIsLoading(true);
  EXPECT_EQ(raw_page_node, obs.TakeNotifiedPageNode());

  EXPECT_CALL(obs, OnUkmSourceIdChanged(_))
      .WillOnce(Invoke(&obs, &MockObserver::SetNotifiedPageNode));
  page_node->SetUkmSourceId(static_cast<ukm::SourceId>(0x1234));
  EXPECT_EQ(raw_page_node, obs.TakeNotifiedPageNode());

  EXPECT_CALL(obs, OnPageLifecycleStateChanged(_))
      .WillOnce(Invoke(&obs, &MockObserver::SetNotifiedPageNode));
  page_node->SetLifecycleStateForTesting(PageNodeImpl::LifecycleState::kFrozen);
  EXPECT_EQ(raw_page_node, obs.TakeNotifiedPageNode());

  EXPECT_CALL(obs, OnPageAlmostIdleChanged(_))
      .WillOnce(Invoke(&obs, &MockObserver::SetNotifiedPageNode));
  page_node->SetPageAlmostIdleForTesting(true);
  EXPECT_EQ(raw_page_node, obs.TakeNotifiedPageNode());

  EXPECT_CALL(obs, OnMainFrameNavigationCommitted(_))
      .WillOnce(Invoke(&obs, &MockObserver::SetNotifiedPageNode));
  page_node->OnMainFrameNavigationCommitted(base::TimeTicks::Now(), 0x1234ull,
                                            GURL("https://foo.com/"));
  EXPECT_EQ(raw_page_node, obs.TakeNotifiedPageNode());

  EXPECT_CALL(obs, OnTitleUpdated(_))
      .WillOnce(Invoke(&obs, &MockObserver::SetNotifiedPageNode));
  page_node->OnTitleUpdated();
  EXPECT_EQ(raw_page_node, obs.TakeNotifiedPageNode());

  EXPECT_CALL(obs, OnFaviconUpdated(_))
      .WillOnce(Invoke(&obs, &MockObserver::SetNotifiedPageNode));
  page_node->OnFaviconUpdated();
  EXPECT_EQ(raw_page_node, obs.TakeNotifiedPageNode());

  // Release the page node and expect a call to "OnBeforePageNodeRemoved".
  EXPECT_CALL(obs, OnBeforePageNodeRemoved(_))
      .WillOnce(Invoke(&obs, &MockObserver::SetNotifiedPageNode));
  page_node.reset();
  EXPECT_EQ(raw_page_node, obs.TakeNotifiedPageNode());

  graph()->RemovePageNodeObserver(&obs);
}

TEST_F(PageNodeImplTest, PublicInterface) {
  auto page_node = CreateNode<PageNodeImpl>();
  const PageNode* public_page_node = page_node.get();

  // Simply test that the public interface impls yield the same result as their
  // private counterpart.

  EXPECT_EQ(page_node->page_almost_idle(),
            public_page_node->IsPageAlmostIdle());
  EXPECT_EQ(page_node->is_visible(), public_page_node->IsVisible());
  EXPECT_EQ(page_node->is_audible(), public_page_node->IsAudible());
  EXPECT_EQ(page_node->is_loading(), public_page_node->IsLoading());
  EXPECT_EQ(page_node->ukm_source_id(), public_page_node->GetUkmSourceID());
  EXPECT_EQ(page_node->lifecycle_state(),
            public_page_node->GetLifecycleState());
  EXPECT_EQ(page_node->navigation_id(), public_page_node->GetNavigationID());
  EXPECT_EQ(page_node->main_frame_url(), public_page_node->GetMainFrameUrl());
}

}  // namespace performance_manager
