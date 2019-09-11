// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_TRACING_COORDINATOR_H_
#define SERVICES_TRACING_COORDINATOR_H_

#include <map>
#include <memory>
#include <set>
#include <string>

#include "base/memory/ref_counted.h"
#include "base/memory/weak_ptr.h"
#include "base/sequenced_task_runner.h"
#include "base/single_thread_task_runner.h"
#include "base/timer/timer.h"
#include "base/trace_event/trace_config.h"
#include "base/values.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/system/data_pipe.h"
#include "services/tracing/agent_registry.h"
#include "services/tracing/public/mojom/tracing.mojom.h"
#include "services/tracing/recorder.h"

namespace service_manager {
struct BindSourceInfo;
}  // namespace service_manager

namespace tracing {

// Note that this implementation of mojom::Coordinator assumes that agents
// either respond to messages that expect a response or disconnect. Mojo
// verifies this to some extend by DCHECKing if the callback is deleted by the
// agent before being run. However, the agent should not store the callback and
// never run it.
//
// If we see that the above-mentioned assumption does not hold in some cases, we
// should guard against it using timeouts.
//
// Note that this class is only used when TraceLog is used as the tracing
// backend; when Perfetto is used, PerfettoTracingCoordinator is used instead to
// implement the same interface.
class Coordinator : public mojom::Coordinator {
 public:
  Coordinator(AgentRegistry* agent_registry,
              const base::RepeatingClosure& on_disconnect_callback);

  void BindCoordinatorRequest(
      mojo::PendingReceiver<mojom::Coordinator> request,
      const service_manager::BindSourceInfo& source_info);

  bool IsConnected();

  void AddExpectedPID(base::ProcessId pid);
  void RemoveExpectedPID(base::ProcessId pid);
  void FinishedReceivingRunningPIDs();

 protected:
  ~Coordinator() override;

  static const void* GetStartTracingClosureName();

  virtual void OnClientConnectionError();
  void CallStartTracingCallbackIfNeeded();
  void OnBeginTracingTimeout();
  void SetStartTracingCallback(StartTracingCallback callback);
  void ClearConnectedPIDs();

  base::trace_event::TraceConfig parsed_config_;
  StartTracingCallback start_tracing_callback_;
  const scoped_refptr<base::SequencedTaskRunner> task_runner_;
  AgentRegistry* agent_registry_;

 private:
  friend std::default_delete<Coordinator>;
  friend class CoordinatorTestUtil;  // For testing.

  class TraceStreamer;

  void Reset();

  // mojom::Coordinator
  void StartTracing(const std::string& config,
                    StartTracingCallback callback) override;
  void StopAndFlush(mojo::ScopedDataPipeProducerHandle stream,
                    StopAndFlushCallback callback) override;
  void StopAndFlushAgent(mojo::ScopedDataPipeProducerHandle stream,
                         const std::string& agent_label,
                         StopAndFlushCallback callback) override;
  void IsTracing(IsTracingCallback callback) override;
  void RequestBufferUsage(RequestBufferUsageCallback callback) override;

  // Internal methods for collecting events from agents.
  void SendStartTracingToAgent(AgentRegistry::AgentEntry* agent_entry);
  void OnTracingStarted(AgentRegistry::AgentEntry* agent_entry, bool success);
  void StopAndFlushInternal();
  void SendStopTracingToAgent(AgentRegistry::AgentEntry* agent_entry);
  void SendStopTracingWithNoOpRecorderToAgent(
      AgentRegistry::AgentEntry* agent_entry);
  void SendRecorder(base::WeakPtr<AgentRegistry::AgentEntry> agent_entry,
                    mojom::RecorderPtr recorder);
  void OnFlushDone();

  void OnRequestBufferStatusResponse(AgentRegistry::AgentEntry* agent_entry,
                                     uint32_t capacity,
                                     uint32_t count);

  base::RepeatingClosure on_disconnect_callback_;
  mojo::Receiver<mojom::Coordinator> receiver_{this};
  const scoped_refptr<base::SequencedTaskRunner> backend_task_runner_;
  std::string config_;
  bool is_tracing_ = false;

  std::unique_ptr<TraceStreamer> trace_streamer_;
  std::set<base::ProcessId> pending_connected_pids_;
  bool pending_currently_running_pids_ = true;
  base::OneShotTimer start_tracing_callback_timer_;
  StopAndFlushCallback stop_and_flush_callback_;

  // For computing trace buffer usage.
  float maximum_trace_buffer_usage_ = 0;
  uint32_t approximate_event_count_ = 0;
  RequestBufferUsageCallback request_buffer_usage_callback_;

  base::WeakPtrFactory<Coordinator> weak_ptr_factory_{this};

  DISALLOW_COPY_AND_ASSIGN(Coordinator);
};

}  // namespace tracing
#endif  // SERVICES_TRACING_COORDINATOR_H_
