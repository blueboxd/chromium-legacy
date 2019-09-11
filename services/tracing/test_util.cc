// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/tracing/test_util.h"

#include <string>
#include <utility>

#include "services/tracing/public/mojom/tracing.mojom.h"

namespace tracing {

MockAgent::MockAgent() {}

MockAgent::~MockAgent() = default;

mojo::PendingRemote<mojom::Agent> MockAgent::CreateAgentRemote() {
  mojo::PendingRemote<mojom::Agent> agent_proxy;
  receiver_.Bind(agent_proxy.InitWithNewPipeAndPassReceiver());
  return agent_proxy;
}

void MockAgent::StartTracing(const std::string& config,
                             base::TimeTicks coordinator_time,
                             StartTracingCallback cb) {
  call_stat_.push_back("StartTracing");
  std::move(cb).Run(true);
}

void MockAgent::StopAndFlush(mojom::RecorderPtr recorder) {
  call_stat_.push_back("StopAndFlush");
  if (!metadata_.empty())
    recorder->AddMetadata(metadata_.Clone());
  for (const auto& chunk : data_) {
    recorder->AddChunk(chunk);
  }
}

void MockAgent::RequestBufferStatus(RequestBufferStatusCallback cb) {
  call_stat_.push_back("RequestBufferStatus");
  std::move(cb).Run(trace_log_status_.event_capacity,
                    trace_log_status_.event_count);
}

}  // namespace tracing
