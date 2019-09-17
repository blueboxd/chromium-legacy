// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_TRACING_CAST_TRACING_AGENT_H_
#define CONTENT_BROWSER_TRACING_CAST_TRACING_AGENT_H_

#include <memory>
#include <string>

#include "chromecast/tracing/system_tracer.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "services/tracing/public/cpp/base_agent.h"
#include "services/tracing/public/mojom/tracing.mojom.h"

namespace chromecast {
class SystemTracer;
}

namespace content {

class CastSystemTracingSession;

// TODO(crbug.com/839086): Remove once we have replaced the legacy tracing
// service with perfetto.
class CastTracingAgent : public tracing::BaseAgent {
 public:
  CastTracingAgent();
  ~CastTracingAgent() override;

 private:
  // tracing::BaseAgent implementation.
  void GetCategories(std::set<std::string>* category_set) override;

  // tracing::mojom::Agent. Called by Mojo internals on the UI thread.
  void StartTracing(const std::string& config,
                    base::TimeTicks coordinator_time,
                    Agent::StartTracingCallback callback) override;
  void StopAndFlush(
      mojo::PendingRemote<tracing::mojom::Recorder> recorder) override;

  void StartTracingCallbackProxy(Agent::StartTracingCallback callback,
                                 bool success);
  void HandleTraceData(chromecast::SystemTracer::Status status,
                       std::string trace_data);

  mojo::Remote<tracing::mojom::Recorder> recorder_;

  // Task runner for collecting traces in a worker thread.
  scoped_refptr<base::SequencedTaskRunner> worker_task_runner_;

  std::unique_ptr<CastSystemTracingSession> session_;

  DISALLOW_COPY_AND_ASSIGN(CastTracingAgent);
};

}  // namespace content

#endif  // CONTENT_BROWSER_TRACING_CAST_TRACING_AGENT_H_
