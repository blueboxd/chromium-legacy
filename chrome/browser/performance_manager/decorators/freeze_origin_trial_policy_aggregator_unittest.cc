// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/performance_manager/decorators/freeze_origin_trial_policy_aggregator.h"

#include "chrome/browser/performance_manager/graph/frame_node_impl.h"
#include "chrome/browser/performance_manager/graph/graph_impl_operations.h"
#include "chrome/browser/performance_manager/graph/process_node_impl.h"
#include "chrome/browser/performance_manager/public/graph/page_node.h"
#include "chrome/browser/performance_manager/test_support/graph_test_harness.h"
#include "services/resource_coordinator/public/mojom/coordination_unit.mojom.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace performance_manager {

using resource_coordinator::mojom::InterventionPolicy;

namespace {

class FreezeOriginTrialPolicyAggregatorTest : public GraphTestHarness {
 public:
  void SetUp() override {
    GraphTestHarness::SetUp();
    graph()->PassToGraph(std::make_unique<FreezeOriginTrialPolicyAggregator>());
  }
};

void ExpectInitialPolicyWorks(GraphImpl* mock_graph,
                              InterventionPolicy f0_policy,
                              InterventionPolicy f1_policy,
                              InterventionPolicy f0_policy_aggregated,
                              InterventionPolicy f0f1_policy_aggregated) {
  TestNodeWrapper<ProcessNodeImpl> process =
      TestNodeWrapper<ProcessNodeImpl>::Create(mock_graph);
  TestNodeWrapper<PageNodeImpl> page =
      TestNodeWrapper<PageNodeImpl>::Create(mock_graph);

  // Check the initial values before any frames are added.
  EXPECT_EQ(InterventionPolicy::kDefault, page->origin_trial_freeze_policy());

  // Create an initial frame. Expect the page policy to be
  // |f0_policy_aggregated| when it is made current.
  TestNodeWrapper<FrameNodeImpl> f0 = TestNodeWrapper<FrameNodeImpl>::Create(
      mock_graph, process.get(), page.get());
  f0->SetOriginTrialFreezePolicy(f0_policy);
  EXPECT_EQ(InterventionPolicy::kDefault, page->origin_trial_freeze_policy());
  f0->SetIsCurrent(true);
  EXPECT_EQ(f0_policy_aggregated, page->origin_trial_freeze_policy());

  // Create a second frame and expect the page policy to be
  // |f0f1_policy_aggregated| when it is made current.
  TestNodeWrapper<FrameNodeImpl> f1 = TestNodeWrapper<FrameNodeImpl>::Create(
      mock_graph, process.get(), page.get(), f0.get(), 1);
  f1->SetOriginTrialFreezePolicy(f1_policy);
  f1->SetIsCurrent(true);
  EXPECT_EQ(f0f1_policy_aggregated, page->origin_trial_freeze_policy());

  // Make the second frame non-current. Expect the page policy to go back to
  // |f0_policy_aggregated|.
  f1->SetIsCurrent(false);
  EXPECT_EQ(f0_policy_aggregated, page->origin_trial_freeze_policy());
  f1->SetIsCurrent(true);
  EXPECT_EQ(f0f1_policy_aggregated, page->origin_trial_freeze_policy());

  // Remove the second frame. Expect the page policy to go back to
  // |f0_policy_aggregated|.
  f1.reset();
  EXPECT_EQ(f0_policy_aggregated, page->origin_trial_freeze_policy());
}

}  // namespace

// Tests all possible combinations of policies for 2 frames. In this test, the
// policy of a frame is set before it becomes current.
TEST_F(FreezeOriginTrialPolicyAggregatorTest, InitialPolicy) {
  auto* mock_graph = graph();

  // Unknown x [Unknown, Default, OptIn, OptOut]

  ExpectInitialPolicyWorks(
      mock_graph, InterventionPolicy::kUnknown /* f0_policy */,
      InterventionPolicy::kUnknown /* f1_policy */,
      InterventionPolicy::kUnknown /* f0_policy_aggregated */,
      InterventionPolicy::kUnknown /* f0f1_policy_aggregated */);

  ExpectInitialPolicyWorks(
      mock_graph, InterventionPolicy::kUnknown /* f0_policy */,
      InterventionPolicy::kDefault /* f1_policy */,
      InterventionPolicy::kUnknown /* f0_policy_aggregated */,
      InterventionPolicy::kUnknown /* f0f1_policy_aggregated */);

  ExpectInitialPolicyWorks(
      mock_graph, InterventionPolicy::kUnknown /* f0_policy */,
      InterventionPolicy::kOptIn /* f1_policy */,
      InterventionPolicy::kUnknown /* f0_policy_aggregated */,
      InterventionPolicy::kUnknown /* f0f1_policy_aggregated */);

  ExpectInitialPolicyWorks(
      mock_graph, InterventionPolicy::kUnknown /* f0_policy */,
      InterventionPolicy::kOptOut /* f1_policy */,
      InterventionPolicy::kUnknown /* f0_policy_aggregated */,
      InterventionPolicy::kUnknown /* f0f1_policy_aggregated */);

  // Default x [Unknown, Default, OptIn, OptOut]

  ExpectInitialPolicyWorks(
      mock_graph, InterventionPolicy::kDefault /* f0_policy */,
      InterventionPolicy::kUnknown /* f1_policy */,
      InterventionPolicy::kDefault /* f0_policy_aggregated */,
      InterventionPolicy::kUnknown /* f0f1_policy_aggregated */);

  ExpectInitialPolicyWorks(
      mock_graph, InterventionPolicy::kDefault /* f0_policy */,
      InterventionPolicy::kDefault /* f1_policy */,
      InterventionPolicy::kDefault /* f0_policy_aggregated */,
      InterventionPolicy::kDefault /* f0f1_policy_aggregated */);

  ExpectInitialPolicyWorks(
      mock_graph, InterventionPolicy::kDefault /* f0_policy */,
      InterventionPolicy::kOptIn /* f1_policy */,
      InterventionPolicy::kDefault /* f0_policy_aggregated */,
      InterventionPolicy::kOptIn /* f0f1_policy_aggregated */);

  ExpectInitialPolicyWorks(
      mock_graph, InterventionPolicy::kDefault /* f0_policy */,
      InterventionPolicy::kOptOut /* f1_policy */,
      InterventionPolicy::kDefault /* f0_policy_aggregated */,
      InterventionPolicy::kOptOut /* f0f1_policy_aggregated */);

  // OptIn x [Unknown, Default, OptIn, OptOut]

  ExpectInitialPolicyWorks(
      mock_graph, InterventionPolicy::kOptIn /* f0_policy */,
      InterventionPolicy::kUnknown /* f1_policy */,
      InterventionPolicy::kOptIn /* f0_policy_aggregated */,
      InterventionPolicy::kUnknown /* f0f1_policy_aggregated */);

  ExpectInitialPolicyWorks(
      mock_graph, InterventionPolicy::kOptIn /* f0_policy */,
      InterventionPolicy::kDefault /* f1_policy */,
      InterventionPolicy::kOptIn /* f0_policy_aggregated */,
      InterventionPolicy::kOptIn /* f0f1_policy_aggregated */);

  ExpectInitialPolicyWorks(
      mock_graph, InterventionPolicy::kOptIn /* f0_policy */,
      InterventionPolicy::kOptIn /* f1_policy */,
      InterventionPolicy::kOptIn /* f0_policy_aggregated */,
      InterventionPolicy::kOptIn /* f0f1_policy_aggregated */);

  ExpectInitialPolicyWorks(
      mock_graph, InterventionPolicy::kOptIn /* f0_policy */,
      InterventionPolicy::kOptOut /* f1_policy */,
      InterventionPolicy::kOptIn /* f0_policy_aggregated */,
      InterventionPolicy::kOptOut /* f0f1_policy_aggregated */);

  // OptOut x [Unknown, Default, OptIn, OptOut]

  ExpectInitialPolicyWorks(
      mock_graph, InterventionPolicy::kOptOut /* f0_policy */,
      InterventionPolicy::kUnknown /* f1_policy */,
      InterventionPolicy::kOptOut /* f0_policy_aggregated */,
      InterventionPolicy::kUnknown /* f0f1_policy_aggregated */);

  ExpectInitialPolicyWorks(
      mock_graph, InterventionPolicy::kOptOut /* f0_policy */,
      InterventionPolicy::kDefault /* f1_policy */,
      InterventionPolicy::kOptOut /* f0_policy_aggregated */,
      InterventionPolicy::kOptOut /* f0f1_policy_aggregated */);

  ExpectInitialPolicyWorks(
      mock_graph, InterventionPolicy::kOptOut /* f0_policy */,
      InterventionPolicy::kOptIn /* f1_policy */,
      InterventionPolicy::kOptOut /* f0_policy_aggregated */,
      InterventionPolicy::kOptOut /* f0f1_policy_aggregated */);

  ExpectInitialPolicyWorks(
      mock_graph, InterventionPolicy::kOptOut /* f0_policy */,
      InterventionPolicy::kOptOut /* f1_policy */,
      InterventionPolicy::kOptOut /* f0_policy_aggregated */,
      InterventionPolicy::kOptOut /* f0f1_policy_aggregated */);
}

// Test changing the policy of a frame after it becomes current.
TEST_F(FreezeOriginTrialPolicyAggregatorTest, PolicyChanges) {
  TestNodeWrapper<ProcessNodeImpl> process =
      TestNodeWrapper<ProcessNodeImpl>::Create(graph());
  TestNodeWrapper<PageNodeImpl> page =
      TestNodeWrapper<PageNodeImpl>::Create(graph());
  TestNodeWrapper<FrameNodeImpl> frame = TestNodeWrapper<FrameNodeImpl>::Create(
      graph(), process.get(), page.get());
  frame->SetIsCurrent(true);

  EXPECT_EQ(InterventionPolicy::kUnknown, page->origin_trial_freeze_policy());
  frame->SetOriginTrialFreezePolicy(InterventionPolicy::kOptIn);
  EXPECT_EQ(InterventionPolicy::kOptIn, page->origin_trial_freeze_policy());
  frame->SetOriginTrialFreezePolicy(InterventionPolicy::kOptOut);
  EXPECT_EQ(InterventionPolicy::kOptOut, page->origin_trial_freeze_policy());
  frame->SetOriginTrialFreezePolicy(InterventionPolicy::kDefault);
  EXPECT_EQ(InterventionPolicy::kDefault, page->origin_trial_freeze_policy());
  frame->SetOriginTrialFreezePolicy(InterventionPolicy::kUnknown);
  EXPECT_EQ(InterventionPolicy::kUnknown, page->origin_trial_freeze_policy());
}

}  // namespace performance_manager
