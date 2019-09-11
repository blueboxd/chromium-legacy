// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/tracing/coordinator_test_util.h"

#include <algorithm>
#include <set>
#include <utility>

#include "base/bind.h"
#include "services/tracing/agent_registry.h"
#include "services/tracing/coordinator.h"
#include "services/tracing/public/mojom/tracing.mojom.h"
#include "services/tracing/test_util.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace tracing {

CoordinatorTestUtil::CoordinatorTestUtil() {}
CoordinatorTestUtil::~CoordinatorTestUtil() {}

// testing::Test
void CoordinatorTestUtil::SetUp() {
  agent_registry_ = std::make_unique<AgentRegistry>();
  output_ = "";
  tracing_begin_callback_received_ = false;
}

// testing::Test
void CoordinatorTestUtil::TearDown() {
  agents_.clear();
  coordinator_.reset();
  agent_registry_.reset();
}

// mojo::DataPipeDrainer::Client
void CoordinatorTestUtil::OnDataAvailable(const void* data, size_t num_bytes) {
  output_.append(static_cast<const char*>(data), num_bytes);
}

// mojo::DataPipeDrainer::Client
void CoordinatorTestUtil::OnDataComplete() {
  std::move(wait_for_data_closure_).Run();
}

MockAgent* CoordinatorTestUtil::AddArrayAgent(base::ProcessId pid) {
  auto agent = std::make_unique<MockAgent>();
  agent_registry_->RegisterAgent(agent->CreateAgentRemote(), "traceEvents",
                                 mojom::TraceDataType::ARRAY, pid);
  agents_.push_back(std::move(agent));
  return agents_.back().get();
}

MockAgent* CoordinatorTestUtil::AddArrayAgent() {
  return AddArrayAgent(base::kNullProcessId);
}

MockAgent* CoordinatorTestUtil::AddObjectAgent() {
  auto agent = std::make_unique<MockAgent>();
  agent_registry_->RegisterAgent(
      agent->CreateAgentRemote(), "systemTraceEvents",
      mojom::TraceDataType::OBJECT, base::kNullProcessId);
  agents_.push_back(std::move(agent));
  return agents_.back().get();
}

MockAgent* CoordinatorTestUtil::AddStringAgent() {
  auto agent = std::make_unique<MockAgent>();
  agent_registry_->RegisterAgent(agent->CreateAgentRemote(), "power",
                                 mojom::TraceDataType::STRING,
                                 base::kNullProcessId);
  agents_.push_back(std::move(agent));
  return agents_.back().get();
}

void CoordinatorTestUtil::StartTracing(std::string config,
                                       bool expected_response) {
  wait_for_data_closure_ = tracing_loop_.QuitClosure();

  coordinator_->StartTracing(
      config, base::BindRepeating(
                  [](bool expected, bool* tracing_begin_callback_received,
                     bool actual) {
                    EXPECT_EQ(expected, actual);
                    *tracing_begin_callback_received = true;
                  },
                  expected_response, &tracing_begin_callback_received_));
}

std::string CoordinatorTestUtil::StopAndFlush() {
  mojo::DataPipe data_pipe;
  auto dummy_callback = [](base::Value metadata) {};
  coordinator_->StopAndFlush(std::move(data_pipe.producer_handle),
                             base::BindRepeating(dummy_callback));
  drainer_.reset(
      new mojo::DataPipeDrainer(this, std::move(data_pipe.consumer_handle)));
  if (!wait_for_data_closure_.is_null())
    tracing_loop_.Run();

  return output_;
}

void CoordinatorTestUtil::IsTracing(bool expected_response) {
  coordinator_->IsTracing(base::BindRepeating(
      [](bool expected, bool actual) { EXPECT_EQ(expected, actual); },
      expected_response));
}

void CoordinatorTestUtil::RequestBufferUsage(float expected_usage,
                                             uint32_t expected_count) {
  coordinator_->RequestBufferUsage(base::BindRepeating(
      [](float expected_usage, uint32_t expected_count, bool success,
         float usage, uint32_t count) {
        EXPECT_TRUE(success);
        EXPECT_EQ(expected_usage, usage);
        EXPECT_EQ(expected_count, count);
      },
      expected_usage, expected_count));
}

void CoordinatorTestUtil::CheckDisconnectClosures(size_t num_agents) {
  // Verify that all disconnect closures are cleared up. This means that, for
  // each agent, either the tracing service is notified that the agent is
  // disconnected or the agent has answered to all requests.
  size_t count = 0;
  agent_registry_->ForAllAgents([&count](AgentRegistry::AgentEntry* entry) {
    count++;
    EXPECT_EQ(0u, entry->num_disconnect_closures_for_testing());
  });
  EXPECT_EQ(num_agents, count);
}

}  // namespace tracing
