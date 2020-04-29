// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/performance_manager/policies/high_pmf_memory_pressure_policy.h"

#include "base/bind.h"
#include "base/memory/memory_pressure_listener.h"
#include "base/run_loop.h"
#include "base/test/bind_test_util.h"
#include "base/util/memory_pressure/multi_source_memory_pressure_monitor.h"
#include "components/performance_manager/graph/graph_impl.h"
#include "components/performance_manager/test_support/graph_test_harness.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace performance_manager {
namespace policies {

using HighPMFMemoryPressurePolicyTest = GraphTestHarness;

TEST_F(HighPMFMemoryPressurePolicyTest, EndToEnd) {
  // Create the policy and pass it to the graph.
  auto policy = std::make_unique<HighPMFMemoryPressurePolicy>();
  auto* policy_raw = policy.get();
  graph()->PassToGraph(std::move(policy));

  const int kPMFLimitKb = 100 * 1024;

  policy_raw->set_pmf_limit_for_testing(kPMFLimitKb);

  // Create a MemoryPressureListener object that will record that a critical
  // memory pressure signal has been emitted.
  base::RunLoop run_loop;
  auto quit_closure = run_loop.QuitClosure();
  bool memory_pressure_listener_called = false;
  util::MultiSourceMemoryPressureMonitor memory_pressure_monitor;
  base::MemoryPressureListener memory_pressure_listener(
      base::BindLambdaForTesting(
          [&](base::MemoryPressureListener::MemoryPressureLevel level) {
            EXPECT_EQ(base::MemoryPressureListener::MemoryPressureLevel::
                          MEMORY_PRESSURE_LEVEL_CRITICAL,
                      level);
            memory_pressure_listener_called = true;
            quit_closure.Run();
          }));

  auto process_node = CreateNode<performance_manager::ProcessNodeImpl>();

  process_node->set_private_footprint_kb(kPMFLimitKb - 1);
  graph()->FindOrCreateSystemNodeImpl()->OnProcessMemoryMetricsAvailable();
  // Make sure that no task get posted to the memory pressure monitor to record
  // a memory pressure event.
  task_env().RunUntilIdle();
  EXPECT_FALSE(memory_pressure_listener_called);
  EXPECT_EQ(base::MemoryPressureListener::MemoryPressureLevel::
                MEMORY_PRESSURE_LEVEL_NONE,
            memory_pressure_monitor.GetCurrentPressureLevel());

  process_node->set_private_footprint_kb(kPMFLimitKb);
  graph()->FindOrCreateSystemNodeImpl()->OnProcessMemoryMetricsAvailable();
  // Wait for the MemoryPressureListener to be called.
  run_loop.Run();
  EXPECT_TRUE(memory_pressure_listener_called);
  EXPECT_EQ(base::MemoryPressureListener::MemoryPressureLevel::
                MEMORY_PRESSURE_LEVEL_CRITICAL,
            memory_pressure_monitor.GetCurrentPressureLevel());

  memory_pressure_listener_called = false;
  process_node->set_private_footprint_kb(kPMFLimitKb - 1);
  graph()->FindOrCreateSystemNodeImpl()->OnProcessMemoryMetricsAvailable();
  // Make sure that the pressure level goes back to NONE.
  task_env().RunUntilIdle();
  EXPECT_FALSE(memory_pressure_listener_called);
  EXPECT_EQ(base::MemoryPressureListener::MemoryPressureLevel::
                MEMORY_PRESSURE_LEVEL_NONE,
            memory_pressure_monitor.GetCurrentPressureLevel());

  graph()->TakeFromGraph(policy_raw);
  // The policy's voter post a task to the UI thread on destruction, wait for
  // this to complete.
  task_env().RunUntilIdle();
}

}  // namespace policies
}  // namespace performance_manager
