// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/tracing/public/cpp/perfetto/perfetto_tracing_backend.h"

#include "base/memory/weak_ptr.h"
#include "base/task/post_task.h"
#include "build/build_config.h"
#include "mojo/public/cpp/system/data_pipe_drainer.h"
#include "services/tracing/public/cpp/perfetto/shared_memory.h"
#include "services/tracing/public/cpp/perfetto/trace_packet_tokenizer.h"
#include "services/tracing/public/mojom/perfetto_service.mojom.h"
#include "services/tracing/public/mojom/tracing_service.mojom.h"
#include "third_party/perfetto/include/perfetto/base/task_runner.h"
#include "third_party/perfetto/include/perfetto/ext/tracing/core/commit_data_request.h"
#include "third_party/perfetto/include/perfetto/ext/tracing/core/consumer.h"
#include "third_party/perfetto/include/perfetto/ext/tracing/core/shared_memory_arbiter.h"
#include "third_party/perfetto/include/perfetto/ext/tracing/core/trace_packet.h"
#include "third_party/perfetto/include/perfetto/ext/tracing/core/trace_stats.h"
#include "third_party/perfetto/include/perfetto/ext/tracing/core/trace_writer.h"
#include "third_party/perfetto/include/perfetto/ext/tracing/core/tracing_service.h"
#include "third_party/perfetto/include/perfetto/tracing/core/trace_config.h"

namespace tracing {

// Implements Perfetto's ConsumerEndpoint interface on top of the
// ConsumerHost mojo service.
class ConsumerEndpoint : public perfetto::ConsumerEndpoint,
                         public mojom::TracingSessionClient,
                         public mojo::DataPipeDrainer::Client {
 public:
  ConsumerEndpoint(PerfettoTracingBackend::Delegate& delegate,
                   perfetto::Consumer* consumer,
                   perfetto::base::TaskRunner* consumer_task_runner)
      : consumer_{consumer} {
    // To avoid extra thread hops, the consumer's task runner must match where
    // the endpoint is constructed.
    DCHECK(consumer_task_runner->RunsTasksOnCurrentThread());

    auto on_connected = base::BindOnce(
        [](base::WeakPtr<ConsumerEndpoint> weak_endpoint,
           perfetto::base::TaskRunner* consumer_task_runner,
           mojo::PendingRemote<mojom::ConsumerHost> consumer_host_remote) {
          // Called on the connection's sequence -- |this| may have been
          // deleted.
          auto wrapped_remote =
              std::make_unique<mojo::PendingRemote<mojom::ConsumerHost>>(
                  std::move(consumer_host_remote));

          // Bind the interfaces on Perfetto's sequence so we can avoid extra
          // thread hops.
          consumer_task_runner->PostTask(
              [weak_endpoint, raw_remote = wrapped_remote.release()]() {
                auto consumer_host_remote = base::WrapUnique(raw_remote);
                // Called on the endpoint's sequence -- |endpoint| may be
                // deleted.
                if (!weak_endpoint)
                  return;
                DCHECK_CALLED_ON_VALID_SEQUENCE(
                    weak_endpoint->sequence_checker_);
                weak_endpoint->consumer_host_.Bind(
                    std::move(*consumer_host_remote));
                weak_endpoint->consumer_host_.reset_on_disconnect();
                weak_endpoint->consumer_->OnConnect();
              });
        },
        weak_factory_.GetWeakPtr(), consumer_task_runner);
    delegate.CreateConsumerConnection(std::move(on_connected));
  }

  ~ConsumerEndpoint() override {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    consumer_->OnDisconnect();
  }

  // perfetto::ConsumerEndpoint implementation.
  void EnableTracing(const perfetto::TraceConfig& trace_config,
                     perfetto::base::ScopedFile file) override {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    DCHECK(!file);  // Direct tracing to a file isn't supported.
    trace_config_ = trace_config;
    if (!trace_config.deferred_start())
      StartTracing();
  }

  void ChangeTraceConfig(const perfetto::TraceConfig& trace_config) override {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    trace_config_ = trace_config;
    tracing_session_host_->ChangeTraceConfig(trace_config);
  }

  void StartTracing() override {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    // TODO(skyostil): Don't hardcode the session's priority.
    consumer_host_->EnableTracing(
        tracing_session_host_.BindNewPipeAndPassReceiver(),
        tracing_session_client_.BindNewPipeAndPassRemote(), trace_config_,
        tracing::mojom::TracingClientPriority::kUserInitiated);
    tracing_session_host_.set_disconnect_handler(base::BindOnce(
        &ConsumerEndpoint::OnTracingFailed, base::Unretained(this)));
    tracing_session_client_.set_disconnect_handler(base::BindOnce(
        &ConsumerEndpoint::OnTracingFailed, base::Unretained(this)));
  }

  void DisableTracing() override {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    tracing_session_host_->DisableTracing();
  }

  void Flush(uint32_t timeout_ms, FlushCallback callback) override {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    // TODO(skyostil): Implement flushing.
    NOTREACHED();
  }

  void ReadBuffers() override {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    if (!tracing_session_host_ || !tracing_session_client_.is_bound()) {
      OnTracingFailed();
      return;
    }
    mojo::ScopedDataPipeProducerHandle producer_handle;
    mojo::ScopedDataPipeConsumerHandle consumer_handle;
    MojoResult result =
        mojo::CreateDataPipe(nullptr, &producer_handle, &consumer_handle);
    if (result != MOJO_RESULT_OK) {
      OnTracingFailed();
      return;
    }
    drainer_ = std::make_unique<mojo::DataPipeDrainer>(
        this, std::move(consumer_handle));
    tokenizer_ = std::make_unique<TracePacketTokenizer>();
    read_buffers_complete_ = false;
    tracing_session_host_->ReadBuffers(
        std::move(producer_handle),
        base::BindOnce(&ConsumerEndpoint::OnReadBuffersComplete,
                       base::Unretained(this)));
  }

  void FreeBuffers() override {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    tracing_session_host_.reset();
    tracing_session_client_.reset();
    drainer_.reset();
    tokenizer_.reset();
  }

  void Detach(const std::string& key) override {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    NOTREACHED() << "Detaching session not supported";
  }

  void Attach(const std::string& key) override {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    NOTREACHED() << "Attaching session not supported";
  }

  void GetTraceStats() override {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    // Capturing |this| is safe, because the callback will be cancelled if the
    // connection terminates.
    tracing_session_host_->RequestBufferUsage(base::BindOnce(
        [](ConsumerEndpoint* endpoint, bool success, float percent_full,
           bool data_loss) {
          DCHECK_CALLED_ON_VALID_SEQUENCE(endpoint->sequence_checker_);
          // Since we only get a few basic stats from the service, synthesize
          // just enough trace statistics to be able to show a buffer usage
          // indicator.
          // TODO(skyostil): Plumb the entire TraceStats objects from the
          // service to avoid this.
          uint64_t buffer_size = 0;
          if (endpoint->trace_config_.buffers_size() >= 1) {
            buffer_size = endpoint->trace_config_.buffers()[0].size_kb() * 1024;
          }
          perfetto::TraceStats stats;
          if (success && buffer_size) {
            auto* buf_stats = stats.add_buffer_stats();
            buf_stats->set_buffer_size(buffer_size);
            buf_stats->set_bytes_written(
                static_cast<uint64_t>(percent_full * buffer_size));
            if (data_loss)
              buf_stats->set_trace_writer_packet_loss(1);
          }
          endpoint->consumer_->OnTraceStats(success, stats);
        },
        base::Unretained(this)));
  }

  void ObserveEvents(uint32_t events_mask) override {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    DCHECK(!(events_mask &
             ~perfetto::ObservableEvents::TYPE_DATA_SOURCES_INSTANCES));
    observed_events_mask_ = events_mask;
  }

  void QueryServiceState(QueryServiceStateCallback) override {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    // TODO(skyostil): Implement service state querying.
    NOTREACHED();
  }

  void QueryCapabilities(QueryCapabilitiesCallback) override {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    // TODO(skyostil): Implement capability querying.
    NOTREACHED();
  }

  // tracing::mojom::TracingSessionClient implementation:
  void OnTracingEnabled() override {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    // TODO(skyostil): Wire up full data source state. For now Perfetto just
    // needs to know all data sources have started.
    if (observed_events_mask_ &
        perfetto::ObservableEvents::TYPE_DATA_SOURCES_INSTANCES) {
      perfetto::ObservableEvents events;
      events.add_instance_state_changes()->set_state(
          perfetto::ObservableEvents::DATA_SOURCE_INSTANCE_STATE_STARTED);
      consumer_->OnObservableEvents(events);
    }
  }

  void OnTracingDisabled() override {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    // TODO(skyostil): Wire up full data source state. For now Perfetto just
    // needs to know all data sources have stopped.
    if (observed_events_mask_ &
        perfetto::ObservableEvents::TYPE_DATA_SOURCES_INSTANCES) {
      perfetto::ObservableEvents events;
      events.add_instance_state_changes()->set_state(
          perfetto::ObservableEvents::DATA_SOURCE_INSTANCE_STATE_STOPPED);
      consumer_->OnObservableEvents(events);
    }
    consumer_->OnTracingDisabled();
  }

  // mojo::DataPipeDrainer::Client implementation:
  void OnDataAvailable(const void* data, size_t num_bytes) override {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    auto packets =
        tokenizer_->Parse(reinterpret_cast<const uint8_t*>(data), num_bytes);
    if (!packets.empty())
      consumer_->OnTraceData(std::move(packets), /*has_more=*/true);
  }

  void OnDataComplete() override {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    DCHECK(!tokenizer_->has_more());
    tokenizer_.reset();
    MaybeFinishTraceData();
  }

 private:
  void OnTracingFailed() {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    // TODO(skyostil): Inform the crew.
    tracing_session_host_.reset();
    tracing_session_client_.reset();
    drainer_.reset();
    tokenizer_.reset();
  }

  void OnReadBuffersComplete() {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    read_buffers_complete_ = true;
    MaybeFinishTraceData();
  }

  void MaybeFinishTraceData() {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    if (!read_buffers_complete_ || tokenizer_)
      return;
    consumer_->OnTraceData({}, /*has_more=*/false);
  }

  SEQUENCE_CHECKER(sequence_checker_);
  perfetto::Consumer* const consumer_;
  mojo::Remote<tracing::mojom::ConsumerHost> consumer_host_;
  mojo::Remote<tracing::mojom::TracingSessionHost> tracing_session_host_;
  mojo::Receiver<tracing::mojom::TracingSessionClient> tracing_session_client_{
      this};
  std::unique_ptr<mojo::DataPipeDrainer> drainer_;
  perfetto::TraceConfig trace_config_;

  std::unique_ptr<TracePacketTokenizer> tokenizer_;
  bool read_buffers_complete_ = false;
  uint32_t observed_events_mask_ = 0;

  base::WeakPtrFactory<ConsumerEndpoint> weak_factory_{this};
};

PerfettoTracingBackend::PerfettoTracingBackend(Delegate& delegate)
    : delegate_(delegate) {}

PerfettoTracingBackend::~PerfettoTracingBackend() = default;
PerfettoTracingBackend::Delegate::~Delegate() = default;

std::unique_ptr<perfetto::ConsumerEndpoint>
PerfettoTracingBackend::ConnectConsumer(const ConnectConsumerArgs& args) {
  return std::make_unique<ConsumerEndpoint>(delegate_, args.consumer,
                                            args.task_runner);
}

std::unique_ptr<perfetto::ProducerEndpoint>
PerfettoTracingBackend::ConnectProducer(const ConnectProducerArgs& args) {
  // TODO(skyostil): Implement producer endpoint.
  return nullptr;
}

}  // namespace tracing