// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_TRACING_CROS_TRACING_AGENT_H_
#define CONTENT_BROWSER_TRACING_CROS_TRACING_AGENT_H_

#include <memory>
#include <string>

#include "mojo/public/cpp/bindings/remote.h"
#include "services/tracing/public/cpp/base_agent.h"
#include "services/tracing/public/mojom/tracing.mojom.h"

namespace base {
class RefCountedString;
}  // namespace base

namespace content {

class CrOSSystemTracingSession;

// TODO(crbug.com/839086): Remove once we have replaced the legacy tracing
// service with perfetto.
class CrOSTracingAgent : public tracing::BaseAgent {
 public:
  CrOSTracingAgent();

 private:
  friend std::default_delete<CrOSTracingAgent>;

  ~CrOSTracingAgent() override;

  // tracing::mojom::Agent. Called by Mojo internals on the UI thread.
  void StartTracing(const std::string& config,
                    base::TimeTicks coordinator_time,
                    Agent::StartTracingCallback callback) override;
  void StopAndFlush(
      mojo::PendingRemote<tracing::mojom::Recorder> recorder) override;

  void StartTracingCallbackProxy(Agent::StartTracingCallback callback,
                                 bool success);
  void RecorderProxy(const scoped_refptr<base::RefCountedString>& events);

  std::unique_ptr<CrOSSystemTracingSession> session_;
  mojo::Remote<tracing::mojom::Recorder> recorder_;

  DISALLOW_COPY_AND_ASSIGN(CrOSTracingAgent);
};

}  // namespace content

#endif  // CONTENT_BROWSER_TRACING_CROS_TRACING_AGENT_H_
