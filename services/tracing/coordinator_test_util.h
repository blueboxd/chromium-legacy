// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_TRACING_COORDINATOR_TEST_UTIL_H_
#define SERVICES_TRACING_COORDINATOR_TEST_UTIL_H_

#include <memory>
#include <string>
#include <vector>

#include "base/run_loop.h"
#include "base/test/scoped_task_environment.h"
#include "mojo/public/cpp/system/data_pipe.h"
#include "mojo/public/cpp/system/data_pipe_drainer.h"

namespace tracing {

class AgentRegistry;
class Coordinator;
class MockAgent;

class CoordinatorTestUtil : public mojo::DataPipeDrainer::Client {
 public:
  CoordinatorTestUtil();
  ~CoordinatorTestUtil() override;

  void SetUp();
  void TearDown();

  // mojo::DataPipeDrainer::Client
  void OnDataAvailable(const void* data, size_t num_bytes) override;

  // mojo::DataPipeDrainer::Client
  void OnDataComplete() override;

  MockAgent* AddArrayAgent(base::ProcessId pid);
  MockAgent* AddArrayAgent();
  MockAgent* AddObjectAgent();
  MockAgent* AddStringAgent();

  void StartTracing(std::string config, bool expected_response);
  std::string StopAndFlush();

  void IsTracing(bool expected_response);

  void RequestBufferUsage(float expected_usage, uint32_t expected_count);

  void CheckDisconnectClosures(size_t num_agents);

  AgentRegistry* agent_registry() { return agent_registry_.get(); }
  bool tracing_begin_callback_received() const {
    return tracing_begin_callback_received_;
  }

 protected:
  std::unique_ptr<Coordinator> coordinator_;

 private:
  base::test::TaskEnvironment task_environment_;

  std::unique_ptr<AgentRegistry> agent_registry_;
  std::vector<std::unique_ptr<MockAgent>> agents_;
  std::unique_ptr<mojo::DataPipeDrainer> drainer_;

  base::RunLoop tracing_loop_;
  base::RepeatingClosure wait_for_data_closure_;
  std::string output_;
  bool tracing_begin_callback_received_ = false;
};

}  // namespace tracing

#endif  // SERVICES_TRACING_COORDINATOR_TEST_UTIL_H_
