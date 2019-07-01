// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/tracing/cast_tracing_agent.h"

#include <utility>

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/logging.h"
#include "base/no_destructor.h"
#include "base/sequence_checker.h"
#include "base/task/post_task.h"
#include "base/trace_event/trace_config.h"
#include "chromecast/tracing/system_tracing_common.h"
#include "content/public/browser/browser_thread.h"
#include "services/service_manager/public/cpp/connector.h"
#include "services/tracing/public/cpp/perfetto/perfetto_producer.h"
#include "services/tracing/public/cpp/perfetto/perfetto_traced_process.h"
#include "services/tracing/public/mojom/constants.mojom.h"
#include "services/tracing/public/mojom/perfetto_service.mojom.h"
#include "third_party/perfetto/include/perfetto/ext/tracing/core/trace_writer.h"
#include "third_party/perfetto/protos/perfetto/trace/chrome/chrome_trace_event.pbzero.h"
#include "third_party/perfetto/protos/perfetto/trace/trace_packet.pbzero.h"

namespace content {
namespace {

std::string GetTracingCategories(
    const base::trace_event::TraceConfig& trace_config) {
  std::vector<base::StringPiece> categories;
  for (size_t i = 0; i < chromecast::tracing::kCategoryCount; ++i) {
    base::StringPiece category(chromecast::tracing::kCategories[i]);
    if (trace_config.category_filter().IsCategoryGroupEnabled(category))
      categories.push_back(category);
  }
  return base::JoinString(categories, ",");
}

void DestroySystemTracerOnWorker(
    std::unique_ptr<chromecast::SystemTracer> tracer) {}

}  // namespace

class CastSystemTracingSession {
 public:
  using SuccessCallback = base::OnceCallback<void(bool)>;
  using TraceDataCallback =
      base::RepeatingCallback<void(chromecast::SystemTracer::Status status,
                                   std::string trace_data)>;

  CastSystemTracingSession(
      const scoped_refptr<base::SequencedTaskRunner>& worker_task_runner)
      : worker_task_runner_(worker_task_runner) {
    DETACH_FROM_SEQUENCE(worker_sequence_checker_);
  }

  ~CastSystemTracingSession() {
    worker_task_runner_->PostTask(FROM_HERE,
                                  base::BindOnce(&DestroySystemTracerOnWorker,
                                                 std::move(system_tracer_)));
  }

  // Begin tracing if configured in |config|. Calls |success_callback| on the
  // current sequence with |true| if tracing was started and |false| otherwise.
  void StartTracing(const std::string& config, SuccessCallback callback) {
    base::trace_event::TraceConfig trace_config(config);

    if (!trace_config.IsSystraceEnabled()) {
      std::move(callback).Run(false /* success */);
      return;
    }

    worker_task_runner_->PostTask(
        FROM_HERE,
        base::BindOnce(
            &CastSystemTracingSession::StartTracingOnWorker,
            base::Unretained(this), base::SequencedTaskRunnerHandle::Get(),
            GetTracingCategories(trace_config), std::move(callback)));
  }

  // Stops the active tracing session, calls |callback| on the current sequence
  // at least once but possibly multiple times until all data was collected.
  void StopTracing(TraceDataCallback callback) {
    worker_task_runner_->PostTask(
        FROM_HERE,
        base::BindOnce(&CastSystemTracingSession::StopAndFlushOnWorker,
                       base::Unretained(this),
                       base::SequencedTaskRunnerHandle::Get(), callback));
  }

 private:
  void StartTracingOnWorker(scoped_refptr<base::TaskRunner> reply_task_runner,
                            const std::string& categories,
                            SuccessCallback callback) {
    DCHECK_CALLED_ON_VALID_SEQUENCE(worker_sequence_checker_);
    DCHECK(!is_tracing_);
    system_tracer_ = chromecast::SystemTracer::Create();
    system_tracer_->StartTracing(
        categories,
        base::BindOnce(&CastSystemTracingSession::FinishStartOnWorker,
                       base::Unretained(this), reply_task_runner,
                       std::move(callback)));
  }

  void FinishStartOnWorker(scoped_refptr<base::TaskRunner> reply_task_runner,
                           SuccessCallback callback,
                           chromecast::SystemTracer::Status status) {
    DCHECK_CALLED_ON_VALID_SEQUENCE(worker_sequence_checker_);
    is_tracing_ = status == chromecast::SystemTracer::Status::OK;
    if (callback) {
      reply_task_runner->PostTask(
          FROM_HERE, base::BindOnce(std::move(callback), is_tracing_));
    }
  }

  void StopAndFlushOnWorker(scoped_refptr<base::TaskRunner> reply_task_runner,
                            TraceDataCallback callback) {
    if (!is_tracing_) {
      reply_task_runner->PostTask(
          FROM_HERE,
          base::BindOnce(callback, chromecast::SystemTracer::Status::OK,
                         std::string()));
      return;
    }
    DCHECK_CALLED_ON_VALID_SEQUENCE(worker_sequence_checker_);
    system_tracer_->StopTracing(base::BindRepeating(
        &CastSystemTracingSession::HandleTraceDataOnWorker,
        base::Unretained(this), reply_task_runner, std::move(callback)));
  }

  void HandleTraceDataOnWorker(
      scoped_refptr<base::TaskRunner> reply_task_runner,
      TraceDataCallback callback,
      chromecast::SystemTracer::Status status,
      std::string trace_data) {
    DCHECK_CALLED_ON_VALID_SEQUENCE(worker_sequence_checker_);
    reply_task_runner->PostTask(
        FROM_HERE, base::BindOnce(callback, status, std::move(trace_data)));
  }

  SEQUENCE_CHECKER(worker_sequence_checker_);

  // Task runner for collecting traces in a worker thread.
  scoped_refptr<base::SequencedTaskRunner> worker_task_runner_;

  bool is_tracing_ = false;
  std::unique_ptr<chromecast::SystemTracer> system_tracer_;

  DISALLOW_COPY_AND_ASSIGN(CastSystemTracingSession);
};

namespace {

using ChromeEventBundleHandle =
    protozero::MessageHandle<perfetto::protos::pbzero::ChromeEventBundle>;

class CastDataSource : public tracing::PerfettoTracedProcess::DataSourceBase {
 public:
  static CastDataSource* GetInstance() {
    static base::NoDestructor<CastDataSource> instance;
    return instance.get();
  }

  // Called from the tracing::PerfettoProducer on its sequence.
  void StartTracing(
      tracing::PerfettoProducer* producer,
      const perfetto::DataSourceConfig& data_source_config) override {
    DCHECK_CALLED_ON_VALID_SEQUENCE(perfetto_sequence_checker_);
    DCHECK(!session_);
    target_buffer_ = data_source_config.target_buffer();
    session_ = std::make_unique<CastSystemTracingSession>(worker_task_runner_);
    session_->StartTracing(data_source_config.chrome_config().trace_config(),
                           base::BindOnce(&CastDataSource::SystemTracerStarted,
                                          base::Unretained(this)));
  }

  // Called from the tracing::PerfettoProducer on its sequence.
  void StopTracing(base::OnceClosure stop_complete_callback) override {
    DCHECK_CALLED_ON_VALID_SEQUENCE(perfetto_sequence_checker_);
    DCHECK(producer_);
    DCHECK(session_);
    if (!session_started_) {
      session_started_callback_ =
          base::BindOnce(&CastDataSource::StopTracing, base::Unretained(this),
                         std::move(stop_complete_callback));
      return;
    }

    trace_writer_ = producer_->CreateTraceWriter(target_buffer_);
    DCHECK(trace_writer_);
    stop_complete_callback_ = std::move(stop_complete_callback);
    session_->StopTracing(base::BindRepeating(&CastDataSource::OnTraceData,
                                              base::Unretained(this)));
  }

  void Flush(base::RepeatingClosure flush_complete_callback) override {
    // Cast's SystemTracer doesn't currently support flushing while recording.
    flush_complete_callback.Run();
  }

 private:
  friend class base::NoDestructor<CastDataSource>;

  CastDataSource()
      : DataSourceBase(tracing::mojom::kSystemTraceDataSourceName),
        worker_task_runner_(base::CreateSequencedTaskRunnerWithTraits(
            {base::MayBlock(), base::TaskPriority::BEST_EFFORT,
             base::TaskShutdownBehavior::SKIP_ON_SHUTDOWN})) {
    DETACH_FROM_SEQUENCE(perfetto_sequence_checker_);
  }

  void SystemTracerStarted(bool success) {
    DCHECK_CALLED_ON_VALID_SEQUENCE(perfetto_sequence_checker_);
    session_started_ = true;
    if (session_started_callback_)
      std::move(session_started_callback_).Run();
  }

  void OnTraceData(chromecast::SystemTracer::Status status,
                   std::string trace_data) {
    DCHECK_CALLED_ON_VALID_SEQUENCE(perfetto_sequence_checker_);

    if (!stop_complete_callback_)
      return;
    DCHECK(trace_writer_);
    DCHECK(session_);

    if (status != chromecast::SystemTracer::Status::FAIL) {
      perfetto::TraceWriter::TracePacketHandle trace_packet_handle =
          trace_writer_->NewTracePacket();
      ChromeEventBundleHandle event_bundle =
          ChromeEventBundleHandle(trace_packet_handle->set_chrome_events());
      event_bundle->add_legacy_ftrace_output(trace_data.data(),
                                             trace_data.length());
    }

    if (status != chromecast::SystemTracer::Status::KEEP_GOING) {
      trace_writer_->Flush();
      trace_writer_.reset();
      session_.reset();
      session_started_ = false;
      producer_ = nullptr;
      std::move(stop_complete_callback_).Run();
    }
  }

  SEQUENCE_CHECKER(perfetto_sequence_checker_);

  // Task runner for collecting traces in a worker thread.
  scoped_refptr<base::SequencedTaskRunner> worker_task_runner_;

  std::unique_ptr<CastSystemTracingSession> session_;
  bool session_started_ = false;
  base::OnceClosure session_started_callback_;
  std::unique_ptr<perfetto::TraceWriter> trace_writer_;
  base::OnceClosure stop_complete_callback_;
  uint32_t target_buffer_ = 0;

  DISALLOW_COPY_AND_ASSIGN(CastDataSource);
};

}  // namespace

CastTracingAgent::CastTracingAgent()
    : BaseAgent("systemTraceEvents",
                tracing::mojom::TraceDataType::STRING,
                base::kNullProcessId),
      worker_task_runner_(base::CreateSequencedTaskRunnerWithTraits(
          {base::MayBlock(), base::TaskPriority::BEST_EFFORT,
           base::TaskShutdownBehavior::SKIP_ON_SHUTDOWN})) {
  tracing::PerfettoTracedProcess::Get()->AddDataSource(
      CastDataSource::GetInstance());
}

CastTracingAgent::~CastTracingAgent() = default;

// tracing::mojom::Agent. Called by Mojo internals on the UI thread.
void CastTracingAgent::StartTracing(const std::string& config,
                                    base::TimeTicks coordinator_time,
                                    Agent::StartTracingCallback callback) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  DCHECK(!session_);
  session_ = std::make_unique<CastSystemTracingSession>(worker_task_runner_);
  session_->StartTracing(
      config, base::BindOnce(&CastTracingAgent::StartTracingCallbackProxy,
                             base::Unretained(this), std::move(callback)));
}

void CastTracingAgent::StopAndFlush(tracing::mojom::RecorderPtr recorder) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  // This may be called even if we are not tracing.
  if (!session_)
    return;
  recorder_ = std::move(recorder);
  session_->StopTracing(base::BindRepeating(&CastTracingAgent::HandleTraceData,
                                            base::Unretained(this)));
}

void CastTracingAgent::GetCategories(std::set<std::string>* category_set) {
  for (size_t i = 0; i < chromecast::tracing::kCategoryCount; ++i) {
    category_set->insert(chromecast::tracing::kCategories[i]);
  }
}

void CastTracingAgent::StartTracingCallbackProxy(
    Agent::StartTracingCallback callback,
    bool success) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  if (!success)
    session_.reset();
  std::move(callback).Run(success);
}

void CastTracingAgent::HandleTraceData(chromecast::SystemTracer::Status status,
                                       std::string trace_data) {
  if (recorder_ && status != chromecast::SystemTracer::Status::FAIL)
    recorder_->AddChunk(std::move(trace_data));
  if (status != chromecast::SystemTracer::Status::KEEP_GOING) {
    recorder_.reset();
    session_.reset();
  }
}

}  // namespace content
