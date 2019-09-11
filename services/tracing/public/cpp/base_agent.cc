// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/tracing/public/cpp/base_agent.h"

#include <utility>

#include "base/bind.h"
#include "base/trace_event/trace_log.h"
#include "services/tracing/public/cpp/traced_process_impl.h"
#include "services/tracing/public/mojom/constants.mojom.h"

namespace tracing {

BaseAgent::BaseAgent(const std::string& label,
                     mojom::TraceDataType type,
                     base::ProcessId pid)
    : label_(label), type_(type), pid_(pid) {
  TracedProcessImpl::GetInstance()->RegisterAgent(this);
}

BaseAgent::~BaseAgent() {
  TracedProcessImpl::GetInstance()->UnregisterAgent(this);
}

void BaseAgent::Connect(tracing::mojom::AgentRegistry* agent_registry) {
  mojo::PendingRemote<tracing::mojom::Agent> agent;
  receiver_.Bind(agent.InitWithNewPipeAndPassReceiver());
  receiver_.set_disconnect_handler(
      base::BindRepeating(&BaseAgent::Disconnect, base::Unretained(this)));

  agent_registry->RegisterAgent(std::move(agent), label_, type_, pid_);
}

void BaseAgent::GetCategories(std::set<std::string>* category_set) {}

void BaseAgent::Disconnect() {
  receiver_.reset();

  // If we get disconnected it means the tracing service went down, most likely
  // due to the process dying. In that case, stop any tracing in progress.
  if (base::trace_event::TraceLog::GetInstance()->IsEnabled()) {
    base::trace_event::TraceLog::GetInstance()->CancelTracing(
        base::trace_event::TraceLog::OutputCallback());
  }
}

void BaseAgent::StartTracing(const std::string& config,
                             base::TimeTicks coordinator_time,
                             Agent::StartTracingCallback callback) {
  std::move(callback).Run(true /* success */);
}

void BaseAgent::StopAndFlush(tracing::mojom::RecorderPtr recorder) {}

void BaseAgent::RequestBufferStatus(
    Agent::RequestBufferStatusCallback callback) {
  std::move(callback).Run(0 /* capacity */, 0 /* count */);
}

bool BaseAgent::IsBoundForTesting() const {
  return receiver_.is_bound();
}

}  // namespace tracing
