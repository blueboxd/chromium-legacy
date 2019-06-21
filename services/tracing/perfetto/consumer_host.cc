// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/tracing/perfetto/consumer_host.h"

#include <algorithm>
#include <cstring>
#include <string>
#include <tuple>
#include <utility>
#include <vector>

#include "base/logging.h"
#include "base/stl_util.h"
#include "base/strings/strcat.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"
#include "base/task/post_task.h"
#include "build/build_config.h"
#include "mojo/public/cpp/bindings/strong_binding.h"
#include "mojo/public/cpp/system/wait.h"
#include "services/tracing/perfetto/json_trace_exporter.h"
#include "services/tracing/perfetto/perfetto_service.h"
#include "services/tracing/perfetto/track_event_json_exporter.h"
#include "services/tracing/public/cpp/trace_event_args_whitelist.h"
#include "third_party/perfetto/include/perfetto/ext/tracing/core/observable_events.h"
#include "third_party/perfetto/include/perfetto/ext/tracing/core/trace_packet.h"
#include "third_party/perfetto/include/perfetto/ext/tracing/core/trace_stats.h"
#include "third_party/perfetto/include/perfetto/tracing/core/trace_config.h"
#include "third_party/perfetto/protos/perfetto/config/trace_config.pb.h"

namespace tracing {

namespace {

const int32_t kEnableTracingTimeoutSeconds = 10;

perfetto::TraceConfig AdjustTraceConfig(
    const perfetto::TraceConfig& trace_config) {
  perfetto::TraceConfig trace_config_copy(trace_config);
  // Clock snapshotting is incompatible with chrome's process sandboxing.
  // Telemetry uses its own way of snapshotting clocks anyway.
  auto* builtin_data_sources = trace_config_copy.mutable_builtin_data_sources();
  builtin_data_sources->set_disable_clock_snapshotting(true);
  return trace_config_copy;
}

}  // namespace

class ConsumerHost::StreamWriter {
 public:
  using Slices = std::vector<std::string>;

  static scoped_refptr<base::SequencedTaskRunner> CreateTaskRunner() {
    return base::CreateSequencedTaskRunnerWithTraits(
        {base::WithBaseSyncPrimitives(), base::TaskPriority::BEST_EFFORT});
  }

  StreamWriter(mojo::ScopedDataPipeProducerHandle stream,
               TracingSession::ReadBuffersCallback callback,
               base::OnceClosure disconnect_callback,
               scoped_refptr<base::SequencedTaskRunner> callback_task_runner)
      : stream_(std::move(stream)),
        read_buffers_callback_(std::move(callback)),
        disconnect_callback_(std::move(disconnect_callback)),
        callback_task_runner_(callback_task_runner) {}

  void WriteToStream(std::unique_ptr<Slices> slices, bool has_more) {
    DCHECK(stream_.is_valid());
    for (const auto& slice : *slices) {
      uint32_t write_position = 0;

      while (write_position < slice.size()) {
        uint32_t write_bytes = slice.size() - write_position;

        MojoResult result =
            stream_->WriteData(slice.data() + write_position, &write_bytes,
                               MOJO_WRITE_DATA_FLAG_NONE);

        if (result == MOJO_RESULT_OK) {
          write_position += write_bytes;
          continue;
        }

        if (result == MOJO_RESULT_SHOULD_WAIT) {
          result = mojo::Wait(stream_.get(), MOJO_HANDLE_SIGNAL_WRITABLE);
        }

        if (result != MOJO_RESULT_OK) {
          if (!disconnect_callback_.is_null()) {
            callback_task_runner_->PostTask(FROM_HERE,
                                            std::move(disconnect_callback_));
          }
          return;
        }
      }
    }

    if (!has_more && !read_buffers_callback_.is_null()) {
      callback_task_runner_->PostTask(FROM_HERE,
                                      std::move(read_buffers_callback_));
    }
  }

 private:
  mojo::ScopedDataPipeProducerHandle stream_;
  TracingSession::ReadBuffersCallback read_buffers_callback_;
  base::OnceClosure disconnect_callback_;
  scoped_refptr<base::SequencedTaskRunner> callback_task_runner_;

  DISALLOW_COPY_AND_ASSIGN(StreamWriter);
};

ConsumerHost::TracingSession::TracingSession(
    ConsumerHost* host,
    mojom::TracingSessionHostRequest tracing_session_host,
    mojom::TracingSessionClientPtr tracing_session_client,
    const perfetto::TraceConfig& trace_config,
    mojom::TracingClientPriority priority)
    : host_(host),
      tracing_session_client_(std::move(tracing_session_client)),
      binding_(this, std::move(tracing_session_host)),
      tracing_priority_(priority) {
  host_->service()->RegisterTracingSession(this);

  tracing_session_client_.set_connection_error_handler(base::BindOnce(
      &ConsumerHost::DestructTracingSession, base::Unretained(host)));
  binding_.set_connection_error_handler(base::BindOnce(
      &ConsumerHost::DestructTracingSession, base::Unretained(host)));

  privacy_filtering_enabled_ = false;
  for (const auto& data_source : trace_config.data_sources()) {
    if (data_source.config().chrome_config().privacy_filtering_enabled()) {
      privacy_filtering_enabled_ = true;
    }
  }
#if DCHECK_IS_ON()
  if (privacy_filtering_enabled_) {
    // If enabled, filtering must be enabled for all data sources.
    for (const auto& data_source : trace_config.data_sources()) {
      DCHECK(data_source.config().chrome_config().privacy_filtering_enabled());
    }
  }
#endif
  perfetto::TraceConfig trace_config_copy = AdjustTraceConfig(trace_config);

  filtered_pids_.clear();
  for (const auto& ds_config : trace_config_copy.data_sources()) {
    if (ds_config.config().name() == mojom::kTraceEventDataSourceName) {
      for (const auto& filter : ds_config.producer_name_filter()) {
        base::ProcessId pid;
        if (PerfettoService::ParsePidFromProducerName(filter, &pid)) {
          filtered_pids_.insert(pid);
        }
      }
      break;
    }
  }

  pending_enable_tracing_ack_pids_ = host_->service()->active_service_pids();
  base::EraseIf(*pending_enable_tracing_ack_pids_,
                [this](base::ProcessId pid) { return !IsExpectedPid(pid); });

  host_->consumer_endpoint()->EnableTracing(trace_config_copy);
  MaybeSendEnableTracingAck();

  if (pending_enable_tracing_ack_pids_) {
    // We can't know for sure whether all processes we request to connect to the
    // tracing service will connect back, or if all the connected services will
    // ACK our EnableTracing request eventually, so we'll add a timeout for that
    // case.
    enable_tracing_ack_timer_.Start(
        FROM_HERE, base::TimeDelta::FromSeconds(kEnableTracingTimeoutSeconds),
        this, &ConsumerHost::TracingSession::OnEnableTracingTimeout);
  }
}

ConsumerHost::TracingSession::~TracingSession() {
  host_->service()->UnregisterTracingSession(this);
  if (host_->consumer_endpoint()) {
    host_->consumer_endpoint()->FreeBuffers();
  }
}

void ConsumerHost::TracingSession::OnPerfettoEvents(
    const perfetto::ObservableEvents& events) {
  if (!pending_enable_tracing_ack_pids_) {
    return;
  }

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
    if (!PerfettoService::ParsePidFromProducerName(state_change.producer_name(),
                                                   &pid)) {
      continue;
    }

    pending_enable_tracing_ack_pids_->erase(pid);
  }
  MaybeSendEnableTracingAck();
}

void ConsumerHost::TracingSession::OnActiveServicePidAdded(
    base::ProcessId pid) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (pending_enable_tracing_ack_pids_ && IsExpectedPid(pid)) {
    pending_enable_tracing_ack_pids_->insert(pid);
  }
}

void ConsumerHost::TracingSession::OnActiveServicePidRemoved(
    base::ProcessId pid) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (pending_enable_tracing_ack_pids_) {
    pending_enable_tracing_ack_pids_->erase(pid);
    MaybeSendEnableTracingAck();
  }
}

void ConsumerHost::TracingSession::OnActiveServicePidsInitialized() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  MaybeSendEnableTracingAck();
}

void ConsumerHost::TracingSession::RequestDisableTracing(
    base::OnceClosure on_disabled_callback) {
  DCHECK(!on_disabled_callback_);
  on_disabled_callback_ = std::move(on_disabled_callback);
  DisableTracing();
}

void ConsumerHost::TracingSession::OnEnableTracingTimeout() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (!pending_enable_tracing_ack_pids_) {
    return;
  }

  std::stringstream error;
  error << "Timed out waiting for processes to ack BeginTracing: ";
  for (auto pid : *pending_enable_tracing_ack_pids_) {
    error << pid << " ";
  }
  LOG(ERROR) << error.rdbuf();

  DCHECK(tracing_session_client_);
  tracing_session_client_->OnTracingEnabled();
  pending_enable_tracing_ack_pids_.reset();
}

void ConsumerHost::TracingSession::MaybeSendEnableTracingAck() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (!pending_enable_tracing_ack_pids_ ||
      !pending_enable_tracing_ack_pids_->empty() ||
      !host_->service()->active_service_pids_initialized()) {
    return;
  }

  DCHECK(tracing_session_client_);
  tracing_session_client_->OnTracingEnabled();
  pending_enable_tracing_ack_pids_.reset();
  enable_tracing_ack_timer_.Stop();
}

bool ConsumerHost::TracingSession::IsExpectedPid(base::ProcessId pid) const {
  return filtered_pids_.empty() || base::Contains(filtered_pids_, pid);
}

void ConsumerHost::TracingSession::ChangeTraceConfig(
    const perfetto::TraceConfig& trace_config) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  perfetto::TraceConfig trace_config_copy = AdjustTraceConfig(trace_config);
  host_->consumer_endpoint()->ChangeTraceConfig(trace_config_copy);
}

void ConsumerHost::TracingSession::DisableTracing() {
  host_->consumer_endpoint()->DisableTracing();
}

void ConsumerHost::TracingSession::OnTracingDisabled() {
  DCHECK(tracing_session_client_);

  if (enable_tracing_ack_timer_.IsRunning()) {
    enable_tracing_ack_timer_.FireNow();
  }
  DCHECK(!pending_enable_tracing_ack_pids_);

  tracing_session_client_->OnTracingDisabled();

  if (json_trace_exporter_) {
    host_->consumer_endpoint()->ReadBuffers();
  }

  tracing_enabled_ = false;

  if (on_disabled_callback_) {
    std::move(on_disabled_callback_).Run();
  }
}

void ConsumerHost::TracingSession::OnConsumerClientDisconnected() {
  // The TracingSession will be deleted after this point.
  host_->DestructTracingSession();
}

void ConsumerHost::TracingSession::ReadBuffers(
    mojo::ScopedDataPipeProducerHandle stream,
    ReadBuffersCallback callback) {
  read_buffers_stream_writer_ = base::SequenceBound<StreamWriter>(
      StreamWriter::CreateTaskRunner(), std::move(stream), std::move(callback),
      base::BindOnce(&TracingSession::OnConsumerClientDisconnected,
                     weak_factory_.GetWeakPtr()),
      base::SequencedTaskRunnerHandle::Get());

  host_->consumer_endpoint()->ReadBuffers();
}

void ConsumerHost::TracingSession::RequestBufferUsage(
    RequestBufferUsageCallback callback) {
  if (!request_buffer_usage_callback_.is_null()) {
    std::move(callback).Run(false, 0, false);
    return;
  }

  request_buffer_usage_callback_ = std::move(callback);
  host_->consumer_endpoint()->GetTraceStats();
}

void ConsumerHost::TracingSession::DisableTracingAndEmitJson(
    const std::string& agent_label_filter,
    mojo::ScopedDataPipeProducerHandle stream,
    DisableTracingAndEmitJsonCallback callback) {
  DCHECK(!read_buffers_stream_writer_);

  read_buffers_stream_writer_ = base::SequenceBound<StreamWriter>(
      StreamWriter::CreateTaskRunner(), std::move(stream), std::move(callback),
      base::BindOnce(&TracingSession::OnConsumerClientDisconnected,
                     weak_factory_.GetWeakPtr()),
      base::SequencedTaskRunnerHandle::Get());

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
  if (privacy_filtering_enabled_) {
    auto* trace_log = base::trace_event::TraceLog::GetInstance();
    arg_filter_predicate = trace_log->GetArgumentFilterPredicate();
    metadata_filter_predicate = trace_log->GetMetadataFilterPredicate();
  }
  json_trace_exporter_ = std::make_unique<TrackEventJSONExporter>(
      std::move(arg_filter_predicate), std::move(metadata_filter_predicate),
      base::BindRepeating(&ConsumerHost::TracingSession::OnJSONTraceData,
                          base::Unretained(this)));

  json_trace_exporter_->set_label_filter(agent_label_filter);

  DisableTracing();
}

void ConsumerHost::TracingSession::OnJSONTraceData(
    std::string* json,
    base::DictionaryValue* metadata,
    bool has_more) {
  auto slices = std::make_unique<StreamWriter::Slices>();
  slices->push_back(std::string());
  slices->back().swap(*json);
  read_buffers_stream_writer_.Post(FROM_HERE, &StreamWriter::WriteToStream,
                                   std::move(slices), has_more);

  if (!has_more) {
    read_buffers_stream_writer_.Reset();
  }
}

void ConsumerHost::TracingSession::OnTraceData(
    std::vector<perfetto::TracePacket> packets,
    bool has_more) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (json_trace_exporter_) {
    json_trace_exporter_->OnTraceData(std::move(packets), has_more);
    if (!has_more) {
      json_trace_exporter_.reset();
    }
    return;
  }

  auto copy = std::make_unique<StreamWriter::Slices>();
  for (auto& packet : packets) {
    char* data;
    size_t size;
    std::tie(data, size) = packet.GetProtoPreamble();
    copy->emplace_back(data, size);
    auto& slices = packet.slices();
    for (auto& slice : slices) {
      copy->emplace_back(static_cast<const char*>(slice.start), slice.size);
    }
  }
  read_buffers_stream_writer_.Post(FROM_HERE, &StreamWriter::WriteToStream,
                                   std::move(copy), has_more);
  if (!has_more) {
    read_buffers_stream_writer_.Reset();
  }
}

void ConsumerHost::TracingSession::OnTraceStats(
    bool success,
    const perfetto::TraceStats& stats) {
  if (!request_buffer_usage_callback_) {
    return;
  }

  if (!success || stats.buffer_stats_size() != 1) {
    std::move(request_buffer_usage_callback_).Run(false, 0.0f, false);
    return;
  }

  const perfetto::TraceStats::BufferStats& buf_stats = stats.buffer_stats()[0];
  size_t bytes_in_buffer = buf_stats.bytes_written() - buf_stats.bytes_read() -
                           buf_stats.bytes_overwritten() +
                           buf_stats.padding_bytes_written() -
                           buf_stats.padding_bytes_cleared();
  double percent_full =
      bytes_in_buffer / static_cast<double>(buf_stats.buffer_size());
  percent_full = std::min(std::max(0.0, percent_full), 1.0);
  bool data_loss = buf_stats.chunks_overwritten() > 0 ||
                   buf_stats.chunks_discarded() > 0 ||
                   buf_stats.abi_violations() > 0;
  std::move(request_buffer_usage_callback_).Run(true, percent_full, data_loss);
}

void ConsumerHost::TracingSession::Flush(
    uint32_t timeout,
    base::OnceCallback<void(bool)> callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  flush_callback_ = std::move(callback);
  base::WeakPtr<TracingSession> weak_this = weak_factory_.GetWeakPtr();
  host_->consumer_endpoint()->Flush(timeout, [weak_this](bool success) {
    if (!weak_this) {
      return;
    }

    if (weak_this->flush_callback_) {
      std::move(weak_this->flush_callback_).Run(success);
    }
  });
}

// static
void ConsumerHost::BindConsumerRequest(
    PerfettoService* service,
    mojom::ConsumerHostRequest request,
    const service_manager::BindSourceInfo& source_info) {
  mojo::MakeStrongBinding(std::make_unique<ConsumerHost>(service),
                          std::move(request));
}

ConsumerHost::ConsumerHost(PerfettoService* service) : service_(service) {
  DETACH_FROM_SEQUENCE(sequence_checker_);
  consumer_endpoint_ =
      service_->GetService()->ConnectConsumer(this, 0 /*uid_t*/);
  consumer_endpoint_->ObserveEvents(
      perfetto::TracingService::ConsumerEndpoint::ObservableEventType::
          kDataSourceInstances);
}

ConsumerHost::~ConsumerHost() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  // Make sure the tracing_session is destroyed first, as it keeps a pointer to
  // the ConsumerHost parent and accesses it on destruction.
  tracing_session_.reset();
}

void ConsumerHost::EnableTracing(
    mojom::TracingSessionHostRequest tracing_session_host,
    mojom::TracingSessionClientPtr tracing_session_client,
    const perfetto::TraceConfig& trace_config,
    mojom::TracingClientPriority priority) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(!tracing_session_);

  // We create our new TracingSession async, if the PerfettoService allows
  // us to, after it's stopped any currently running lower or equal priority
  // tracing sessions.
  service_->RequestTracingSession(
      priority,
      base::BindOnce(
          [](base::WeakPtr<ConsumerHost> weak_this,
             mojom::TracingSessionHostRequest tracing_session_host,
             mojom::TracingSessionClientPtr tracing_session_client,
             const perfetto::TraceConfig& trace_config,
             mojom::TracingClientPriority priority) {
            if (!weak_this) {
              return;
            }

            weak_this->tracing_session_ = std::make_unique<TracingSession>(
                weak_this.get(), std::move(tracing_session_host),
                std::move(tracing_session_client), trace_config, priority);
          },
          weak_factory_.GetWeakPtr(), std::move(tracing_session_host),
          std::move(tracing_session_client), trace_config, priority));
}

void ConsumerHost::OnConnect() {}

void ConsumerHost::OnDisconnect() {}

void ConsumerHost::OnTracingDisabled() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (tracing_session_) {
    tracing_session_->OnTracingDisabled();
  }
}

void ConsumerHost::OnTraceData(std::vector<perfetto::TracePacket> packets,
                               bool has_more) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (tracing_session_) {
    tracing_session_->OnTraceData(std::move(packets), has_more);
  }
}

void ConsumerHost::OnObservableEvents(
    const perfetto::ObservableEvents& events) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (tracing_session_) {
    tracing_session_->OnPerfettoEvents(events);
  }
}

void ConsumerHost::OnTraceStats(bool success,
                                const perfetto::TraceStats& stats) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (tracing_session_) {
    tracing_session_->OnTraceStats(success, stats);
  }
}

void ConsumerHost::DestructTracingSession() {
  tracing_session_.reset();
}

}  // namespace tracing
