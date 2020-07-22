// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/performance_manager/policies/high_pmf_discard_policy.h"

#include "base/bind.h"
#include "chrome/browser/performance_manager/test_support/page_discarding_utils.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace performance_manager {
namespace policies {

using HighPMFDiscardPolicyTest = testing::GraphTestHarnessWithMockDiscarder;

TEST_F(HighPMFDiscardPolicyTest, EndToEnd) {
  // Create the policy and pass it to the graph.
  auto policy = std::make_unique<HighPMFDiscardPolicy>();
  auto* policy_raw = policy.get();
  graph()->PassToGraph(std::move(policy));

  const int kPMFLimitKb = 100 * 1024;

  policy_raw->set_pmf_limit_for_testing(kPMFLimitKb);
  auto process_node = CreateNode<performance_manager::ProcessNodeImpl>();

  process_node->set_private_footprint_kb(kPMFLimitKb - 1);
  graph()->FindOrCreateSystemNodeImpl()->OnProcessMemoryMetricsAvailable();
  // Make sure that no task get posted to the discarder.
  task_env().RunUntilIdle();
  ::testing::Mock::VerifyAndClearExpectations(discarder());

  process_node->set_private_footprint_kb(kPMFLimitKb);
  EXPECT_CALL(*discarder(), DiscardPageNodeImpl(page_node()))
      .WillOnce(::testing::Return(true));
  graph()->FindOrCreateSystemNodeImpl()->OnProcessMemoryMetricsAvailable();
  task_env().RunUntilIdle();
  ::testing::Mock::VerifyAndClearExpectations(discarder());

  graph()->TakeFromGraph(policy_raw);
}

}  // namespace policies
}  // namespace performance_manager
