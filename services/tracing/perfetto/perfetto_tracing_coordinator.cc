// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/tracing/perfetto/perfetto_tracing_coordinator.h"

#include <algorithm>
#include <utility>
#include <vector>

#include "base/bind.h"
#include "base/callback.h"
#include "base/command_line.h"
#include "base/numerics/ranges.h"
#include "base/strings/strcat.h"
#include "base/strings/string_number_conversions.h"
#include "base/trace_event/trace_log.h"
#include "build/build_config.h"
#include "components/tracing/common/tracing_switches.h"
#include "mojo/public/cpp/system/data_pipe_utils.h"
#include "services/tracing/perfetto/consumer_host.h"
#include "services/tracing/perfetto/perfetto_service.h"
#include "services/tracing/perfetto/track_event_json_exporter.h"
#include "services/tracing/public/cpp/perfetto/perfetto_config.h"
#include "services/tracing/public/cpp/trace_event_args_whitelist.h"
#include "services/tracing/public/mojom/constants.mojom.h"
#include "services/tracing/public/mojom/perfetto_service.mojom.h"
#include "third_party/perfetto/include/perfetto/ext/tracing/core/consumer.h"
#include "third_party/perfetto/include/perfetto/ext/tracing/core/trace_stats.h"
#include "third_party/perfetto/include/perfetto/ext/tracing/core/tracing_service.h"
#include "third_party/perfetto/include/perfetto/tracing/core/trace_config.h"

namespace tracing {

// A TracingSession acts as a perfetto consumer and is used to wrap all the
// associated state of an on-going tracing session, for easy setup and cleanup.
//
// TODO(eseckler): Make it possible to have multiple active sessions
// concurrently. Also support configuring the output format of a session (JSON
// vs. proto).
class PerfettoTracingCoordinator::TracingSession : public perfetto::Consumer {
 public:
  TracingSession(const base::trace_event::TraceConfig& chrome_config,
                 base::OnceClosure tracing_over_callback,
                 const base::RepeatingCallback<void(base::ProcessId)>&
                     on_pid_started_tracing)
      : tracing_over_callback_(std::move(tracing_over_callback)),
        on_pid_started_tracing_(std::move(on_pid_started_tracing)) {
    // In legacy backend, the trace event agent sets the predicate used by
    // TraceLog. For perfetto backend, ensure that predicate is always set
    // before creating the exporter. The agent can be created later than this
    // point.
    if (base::trace_event::TraceLog::GetInstance()
            ->GetArgumentFilterPredicate()
            .is_null()) {
      base::trace_event::TraceLog::GetInstance()->SetArgumentFilterPredicate(
          base::BindRepeating(&IsTraceEventArgsWhitelisted));
      base::trace_event::TraceLog::GetInstance()->SetMetadataFilterPredicate(
          base::BindRepeating(&IsMetadataWhitelisted));
    }

    JSONTraceExporter::ArgumentFilterPredicate arg_filter_predicate;
    JSONTraceExporter::MetadataFilterPredicate metadata_filter_predicate;
    if (chrome_config.IsArgumentFilterEnabled()) {
      auto* trace_log = base::trace_event::TraceLog::GetInstance();
      arg_filter_predicate = trace_log->GetArgumentFilterPredicate();
      metadata_filter_predicate = trace_log->GetMetadataFilterPredicate();
    }
    auto json_event_callback = base::BindRepeating(
        &TracingSession::OnJSONTraceEventCallback, base::Unretained(this));
    json_trace_exporter_ = std::make_unique<TrackEventJSONExporter>(
        std::move(arg_filter_predicate), std::move(metadata_filter_predicate),
        std::move(json_event_callback));
    perfetto::TracingService* service =
        PerfettoService::GetInstance()->GetService();
    consumer_endpoint_ = service->ConnectConsumer(this, /*uid=*/0);
    consumer_endpoint_->ObserveEvents(
        perfetto::TracingService::ConsumerEndpoint::ObservableEventType::
            kDataSourceInstances);

    // Start tracing.
    auto perfetto_config = CreatePerfettoConfiguration(chrome_config);
    consumer_endpoint_->EnableTracing(perfetto_config);
  }

  ~TracingSession() override {
    if (!stop_and_flush_callback_.is_null()) {
      std::move(stop_and_flush_callback_)
          .Run(base::Value(base::Value::Type::DICTIONARY));
    }

    stream_.reset();
  }

  void ChangeTraceConfig(const base::trace_event::TraceConfig& chrome_config) {
    auto perfetto_config = CreatePerfettoConfiguration(chrome_config);
    consumer_endpoint_->ChangeTraceConfig(perfetto_config);
  }

  perfetto::TraceConfig CreatePerfettoConfiguration(
      const base::trace_event::TraceConfig& chrome_config) {
#if DCHECK_IS_ON()
    base::trace_event::TraceConfig processfilter_stripped_config(chrome_config);
    processfilter_stripped_config.SetProcessFilterConfig(
        base::trace_event::TraceConfig::ProcessFilterConfig());

    // Ensure that the process filter is the only thing that gets changed
    // in a configuration during a tracing session.
    DCHECK((last_config_for_perfetto_.ToString() ==
            base::trace_event::TraceConfig().ToString()) ||
           (last_config_for_perfetto_.ToString() ==
            processfilter_stripped_config.ToString()));
    last_config_for_perfetto_ = processfilter_stripped_config;
#endif

    return GetDefaultPerfettoConfig(chrome_config);
  }

  void StopAndFlush(mojo::ScopedDataPipeProducerHandle stream,
                    const std::string& agent_label,
                    StopAndFlushCallback callback) {
    stream_ = std::move(stream);
    stop_and_flush_callback_ = std::move(callback);
    json_trace_exporter_->set_label_filter(agent_label);
    consumer_endpoint_->DisableTracing();
  }

  void OnJSONTraceEventCallback(std::string* json,
                                base::DictionaryValue* metadata,
                                bool has_more) {
    if (stream_.is_valid()) {
      mojo::BlockingCopyFromString(*json, stream_);
    }

    if (!has_more) {
      DCHECK(!stop_and_flush_callback_.is_null());
      std::move(stop_and_flush_callback_)
          .Run(/*metadata=*/std::move(*metadata));

      std::move(tracing_over_callback_).Run();
      // |this| is now destroyed.
    }
  }

  void RequestBufferUsage(RequestBufferUsageCallback callback) {
    if (!request_buffer_usage_callback_.is_null()) {
      // TODO(eseckler): Reconsider whether we can DCHECK here after refactoring
      // of content's TracingController.
      std::move(callback).Run(false, 0, 0);
      return;
    }
    request_buffer_usage_callback_ = std::move(callback);
    consumer_endpoint_->GetTraceStats();
  }

  // perfetto::Consumer implementation.
  // This gets called by the Perfetto service as control signals,
  // and to send finished protobufs over.
  void OnConnect() override {}

  void OnDisconnect() override {}

  void OnTracingDisabled() override { consumer_endpoint_->ReadBuffers(); }

  void OnTraceData(std::vector<perfetto::TracePacket> packets,
                   bool has_more) override {
    json_trace_exporter_->OnTraceData(std::move(packets), has_more);
  }

  // Consumer Detach / Attach is not used in Chrome.
  void OnDetach(bool success) override { NOTREACHED(); }

  void OnAttach(bool success, const perfetto::TraceConfig&) override {
    NOTREACHED();
  }

  void OnTraceStats(bool success, const perfetto::TraceStats& stats) override {
    DCHECK(request_buffer_usage_callback_);
    if (!success) {
      std::move(request_buffer_usage_callback_).Run(false, 0.0f, 0);
      return;
    }
    DCHECK_EQ(1, stats.buffer_stats_size());
    const perfetto::TraceStats::BufferStats& buf_stats =
        stats.buffer_stats()[0];
    size_t bytes_in_buffer =
        buf_stats.bytes_written() - buf_stats.bytes_read() -
        buf_stats.bytes_overwritten() + buf_stats.padding_bytes_written() -
        buf_stats.padding_bytes_cleared();
    double percent_full =
        bytes_in_buffer / static_cast<double>(buf_stats.buffer_size());
    percent_full = base::ClampToRange(percent_full, 0.0, 1.0);
    // We know the size of data in the buffer, but not the number of events.
    std::move(request_buffer_usage_callback_)
        .Run(true, percent_full, 0 /*approx_event_count*/);
  }

  void OnObservableEvents(const perfetto::ObservableEvents& events) override {
    for (const auto& state_change : events.instance_state_changes()) {
      if (state_change.state() !=
          perfetto::ObservableEvents::DataSourceInstanceStateChange::
              DATA_SOURCE_INSTANCE_STATE_STARTED) {
        continue;
      }

      if (state_change.data_source_name() != mojom::kTraceEventDataSourceName) {
        continue;
      }

      // Attempt to parse the PID out of the producer name.
      base::ProcessId pid;
      if (!PerfettoService::ParsePidFromProducerName(
              state_change.producer_name(), &pid)) {
        continue;
      }

      on_pid_started_tracing_.Run(pid);
    }
  }

 private:
  mojo::ScopedDataPipeProducerHandle stream_;
  std::unique_ptr<JSONTraceExporter> json_trace_exporter_;
  StopAndFlushCallback stop_and_flush_callback_;
  base::OnceClosure tracing_over_callback_;
  RequestBufferUsageCallback request_buffer_usage_callback_;
  base::RepeatingCallback<void(base::ProcessId)> on_pid_started_tracing_;

#if DCHECK_IS_ON()
  base::trace_event::TraceConfig last_config_for_perfetto_;
#endif

  // Keep last to avoid edge-cases where its callbacks come in mid-destruction.
  std::unique_ptr<perfetto::TracingService::ConsumerEndpoint>
      consumer_endpoint_;

  DISALLOW_COPY_AND_ASSIGN(TracingSession);
};

PerfettoTracingCoordinator::PerfettoTracingCoordinator(
    AgentRegistry* agent_registry,
    base::RepeatingClosure on_disconnect_callback)
    : Coordinator(agent_registry, std::move(on_disconnect_callback)) {
  DETACH_FROM_SEQUENCE(sequence_checker_);
}

PerfettoTracingCoordinator::~PerfettoTracingCoordinator() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
}

void PerfettoTracingCoordinator::OnClientConnectionError() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  tracing_session_.reset();
  receiver_.reset();
  Coordinator::OnClientConnectionError();
}

void PerfettoTracingCoordinator::BindCoordinatorReceiver(
    mojo::PendingReceiver<mojom::Coordinator> receiver,
    const service_manager::BindSourceInfo& source_info) {
  receiver_.Bind(std::move(receiver));
  receiver_.set_disconnect_handler(
      base::BindOnce(&PerfettoTracingCoordinator::OnClientConnectionError,
                     base::Unretained(this)));
}

void PerfettoTracingCoordinator::StartTracing(const std::string& config,
                                              StartTracingCallback callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  base::trace_event::TraceConfig new_parsed_config(config);
  bool waiting_for_tracing_start = !start_tracing_callback_.is_null();
  bool configs_are_identical =
      (new_parsed_config.ToString() == parsed_config_.ToString());
  if (waiting_for_tracing_start ||
      (tracing_session_ && configs_are_identical)) {
    std::move(callback).Run(configs_are_identical);
    return;
  }

  base::trace_event::TraceConfig old_parsed_config(parsed_config_);
  parsed_config_ = new_parsed_config;

  if (!tracing_session_) {
    tracing_session_ = std::make_unique<TracingSession>(
        parsed_config_,
        base::BindOnce(&PerfettoTracingCoordinator::OnTracingOverCallback,
                       base::Unretained(this)),
        base::BindRepeating(&PerfettoTracingCoordinator::OnPIDStartedTracing,
                            base::Unretained(this)));

    agent_registry_->SetAgentInitializationCallback(
        base::BindRepeating(
            &PerfettoTracingCoordinator::WaitForAgentToBeginTracing,
            weak_factory_.GetWeakPtr()),
        false /* call_on_new_agents_only */);

  } else {
    agent_registry_->ForAllAgents(
        [this, &old_parsed_config,
         &new_parsed_config](AgentRegistry::AgentEntry* agent_entry) {
          if (!old_parsed_config.process_filter_config().IsEnabled(
                  agent_entry->pid()) &&
              new_parsed_config.process_filter_config().IsEnabled(
                  agent_entry->pid())) {
            WaitForAgentToBeginTracing(agent_entry);
          }
        });

    tracing_session_->ChangeTraceConfig(parsed_config_);
  }

  SetStartTracingCallback(std::move(callback));
}

void PerfettoTracingCoordinator::OnPIDStartedTracing(base::ProcessId pid) {
  agent_registry_->ForAllAgents(
      [this, pid](AgentRegistry::AgentEntry* agent_entry) {
        if (pid == agent_entry->pid()) {
          OnAgentStartedTracing(agent_entry);
        }
      });
}

void PerfettoTracingCoordinator::OnAgentStartedTracing(
    AgentRegistry::AgentEntry* agent_entry) {
  agent_entry->RemoveDisconnectClosure(GetStartTracingClosureName());
  CallStartTracingCallbackIfNeeded();
}

void PerfettoTracingCoordinator::WaitForAgentToBeginTracing(
    AgentRegistry::AgentEntry* agent_entry) {
  if (!agent_entry->pid() ||
      !parsed_config_.process_filter_config().IsEnabled(agent_entry->pid()))
    return;

  // TODO(oysteine): While we're still using the Agent system as a fallback when
  // using Perfetto, rather than the browser directly using a Consumer
  // interface, we have to attempt to linearize with newly connected agents so
  // we only call the BeginTracing callback when we can be sure that all current
  // agents have registered with Perfetto and started tracing if requested. We
  // do this by adding a disconnect closure to all newly connected agents and
  // then wait for Perfetto to tell us their data sources are connected.
  agent_entry->AddDisconnectClosure(
      GetStartTracingClosureName(),
      base::BindOnce(&PerfettoTracingCoordinator::OnAgentStartedTracing,
                     weak_factory_.GetWeakPtr(),
                     base::Unretained(agent_entry)));

  RemoveExpectedPID(agent_entry->pid());
}

void PerfettoTracingCoordinator::OnTracingOverCallback() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(tracing_session_);
  tracing_session_.reset();
}

void PerfettoTracingCoordinator::StopAndFlush(
    mojo::ScopedDataPipeProducerHandle stream,
    StopAndFlushCallback callback) {
  StopAndFlushAgent(std::move(stream), "", std::move(callback));
}

void PerfettoTracingCoordinator::StopAndFlushInternal(
    mojo::ScopedDataPipeProducerHandle stream,
    const std::string& agent_label,
    StopAndFlushCallback callback) {
  if (start_tracing_callback_) {
    // We received a |StopAndFlush| command before receiving |StartTracing| acks
    // from all agents. Let's retry after a delay.
    task_runner_->PostDelayedTask(
        FROM_HERE,
        base::BindOnce(&PerfettoTracingCoordinator::StopAndFlushInternal,
                       weak_factory_.GetWeakPtr(), std::move(stream),
                       agent_label, std::move(callback)),
        base::TimeDelta::FromMilliseconds(
            mojom::kStopTracingRetryTimeMilliseconds));
    return;
  }

  agent_registry_->SetAgentInitializationCallback(
      base::DoNothing(), true /* call_on_new_agents_only */);
  tracing_session_->StopAndFlush(std::move(stream), agent_label,
                                 std::move(callback));
}

void PerfettoTracingCoordinator::StopAndFlushAgent(
    mojo::ScopedDataPipeProducerHandle stream,
    const std::string& agent_label,
    StopAndFlushCallback callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(tracing_session_);

  ClearConnectedPIDs();
  StopAndFlushInternal(std::move(stream), agent_label, std::move(callback));
}

void PerfettoTracingCoordinator::IsTracing(IsTracingCallback callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  std::move(callback).Run(tracing_session_ != nullptr);
}

void PerfettoTracingCoordinator::RequestBufferUsage(
    RequestBufferUsageCallback callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (!tracing_session_) {
    std::move(callback).Run(false, 0, 0);
    return;
  }
  tracing_session_->RequestBufferUsage(std::move(callback));
}

}  // namespace tracing
