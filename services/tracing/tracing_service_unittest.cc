// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>
#include <utility>

#include "base/bind.h"
#include "base/macros.h"
#include "base/run_loop.h"
#include "base/strings/string_number_conversions.h"
#include "base/test/task_environment.h"
#include "base/test/test_simple_task_runner.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "services/tracing/perfetto/test_utils.h"
#include "services/tracing/public/cpp/perfetto/perfetto_config.h"
#include "services/tracing/public/cpp/perfetto/perfetto_platform.h"
#include "services/tracing/public/cpp/perfetto/perfetto_traced_process.h"
#include "services/tracing/public/cpp/traced_process_impl.h"
#include "services/tracing/public/cpp/tracing_features.h"
#include "services/tracing/public/mojom/perfetto_service.mojom.h"
#include "services/tracing/public/mojom/traced_process.mojom.h"
#include "services/tracing/public/mojom/tracing_service.mojom.h"
#include "services/tracing/tracing_service.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/perfetto/include/perfetto/tracing/data_source.h"
#include "third_party/perfetto/include/perfetto/tracing/tracing.h"
#include "third_party/perfetto/protos/perfetto/common/trace_stats.pb.h"
#include "third_party/perfetto/protos/perfetto/trace/test_event.pbzero.h"
#include "third_party/perfetto/protos/perfetto/trace/trace.pb.h"
#include "third_party/perfetto/protos/perfetto/trace/trace_packet.pb.h"
#include "third_party/perfetto/protos/perfetto/trace/trace_packet.pbzero.h"

namespace tracing {
namespace {

// A task runner which can be dynamically redirected to a different task runner.
class ProxyTaskRunner : public base::SequencedTaskRunner {
 public:
  ProxyTaskRunner() = default;

  void set_task_runner(scoped_refptr<base::SequencedTaskRunner> task_runner) {
    task_runner_ = task_runner;
  }

  bool PostDelayedTask(const base::Location& from_here,
                       base::OnceClosure task,
                       base::TimeDelta delay) override {
    return task_runner_->PostDelayedTask(from_here, std::move(task), delay);
  }

  bool PostNonNestableDelayedTask(const base::Location& from_here,
                                  base::OnceClosure task,
                                  base::TimeDelta delay) override {
    return task_runner_->PostNonNestableDelayedTask(from_here, std::move(task),
                                                    delay);
  }

  bool RunsTasksInCurrentSequence() const override {
    return task_runner_->RunsTasksInCurrentSequence();
  }

 private:
  ~ProxyTaskRunner() override = default;

  scoped_refptr<base::SequencedTaskRunner> task_runner_;
};

ProxyTaskRunner* GetProxyTaskRunner() {
  static base::NoDestructor<ProxyTaskRunner> task_runner;
  return task_runner.get();
}

}  // namespace

class TracingServiceTest : public testing::Test {
 public:
  TracingServiceTest() : service_(&perfetto_service_) {
    // Since Perfetto's platform backend can only be initialized once in a
    // process, we give it a task runner that can outlive the per-test task
    // environment.
    auto* proxy_task_runner = GetProxyTaskRunner();
    auto* perfetto_platform =
        PerfettoTracedProcess::Get()->perfetto_platform_for_testing();
    if (!perfetto_platform->did_start_task_runner())
      perfetto_platform->StartTaskRunner(proxy_task_runner);
    proxy_task_runner->set_task_runner(base::ThreadTaskRunnerHandle::Get());

    // Also tell PerfettoTracedProcess to use the current task environment.
    PerfettoTracedProcess::ResetTaskRunnerForTesting(
        base::ThreadTaskRunnerHandle::Get());
    PerfettoTracedProcess::Get()->SetupClientLibrary();
    perfetto_service()->SetActiveServicePidsInitialized();
  }
  ~TracingServiceTest() override = default;

 protected:
  PerfettoService* perfetto_service() { return &perfetto_service_; }
  mojom::TracingService* service() { return &service_; }

  void EnableClientApiConsumer() {
    // Tell PerfettoTracedProcess how to connect to the service. This enables
    // the consumer part of the client API.
    static mojom::TracingService* s_service;
    s_service = service();
    auto factory = []() -> mojom::TracingService& { return *s_service; };
    PerfettoTracedProcess::Get()->SetConsumerConnectionFactory(
        factory, base::SequencedTaskRunnerHandle::Get());
  }

  size_t ReadAndCountTestPackets(perfetto::TracingSession& session) {
    size_t test_packet_count = 0;
    base::RunLoop wait_for_data_loop;
    session.ReadTrace(
        [&wait_for_data_loop, &test_packet_count](
            perfetto::TracingSession::ReadTraceCallbackArgs args) {
          if (args.size) {
            perfetto::protos::Trace trace;
            EXPECT_TRUE(trace.ParseFromArray(args.data, args.size));
            for (const auto& packet : trace.packet()) {
              if (packet.has_for_testing()) {
                EXPECT_EQ(kPerfettoTestString, packet.for_testing().str());
                test_packet_count++;
              }
            }
          }
          if (!args.has_more)
            wait_for_data_loop.Quit();
        });
    wait_for_data_loop.Run();
    return test_packet_count;
  }

 private:
  base::test::TaskEnvironment task_environment_;
  PerfettoService perfetto_service_;
  TracingService service_;

  DISALLOW_COPY_AND_ASSIGN(TracingServiceTest);
};

class TestTracingClient : public mojom::TracingSessionClient {
 public:
  void StartTracing(mojom::TracingService* service,
                    base::OnceClosure on_tracing_enabled) {
    service->BindConsumerHost(consumer_host_.BindNewPipeAndPassReceiver());

    perfetto::TraceConfig perfetto_config =
        tracing::GetDefaultPerfettoConfig(base::trace_event::TraceConfig(""),
                                          /*privacy_filtering_enabled=*/false);

    consumer_host_->EnableTracing(
        tracing_session_host_.BindNewPipeAndPassReceiver(),
        receiver_.BindNewPipeAndPassRemote(), std::move(perfetto_config),
        tracing::mojom::TracingClientPriority::kUserInitiated);

    tracing_session_host_->RequestBufferUsage(
        base::BindOnce([](base::OnceClosure on_response, bool, float,
                          bool) { std::move(on_response).Run(); },
                       std::move(on_tracing_enabled)));
  }

  void StopTracing(base::OnceClosure on_tracing_stopped) {
    tracing_disabled_callback_ = std::move(on_tracing_stopped);
    tracing_session_host_->DisableTracing();
  }

  // tracing::mojom::TracingSessionClient implementation:
  void OnTracingEnabled() override {}
  void OnTracingDisabled() override {
    std::move(tracing_disabled_callback_).Run();
  }

 private:
  base::OnceClosure tracing_disabled_callback_;

  mojo::Remote<mojom::ConsumerHost> consumer_host_;
  mojo::Remote<mojom::TracingSessionHost> tracing_session_host_;
  mojo::Receiver<mojom::TracingSessionClient> receiver_{this};
};

TEST_F(TracingServiceTest, TracingServiceInstantiate) {
  TestTracingClient tracing_client;

  base::RunLoop tracing_started;
  tracing_client.StartTracing(service(), tracing_started.QuitClosure());
  tracing_started.Run();

  base::RunLoop tracing_stopped;
  tracing_client.StopTracing(tracing_stopped.QuitClosure());
  tracing_stopped.Run();
}

TEST_F(TracingServiceTest, PerfettoClientConsumer) {
  // Set up API bindings.
  EnableClientApiConsumer();

  // Register a mock producer with an in-process Perfetto service.
  auto pid = 123;
  size_t kNumPackets = 10;
  base::RunLoop wait_for_start;
  base::RunLoop wait_for_registration;
  std::unique_ptr<MockProducer> producer = std::make_unique<MockProducer>(
      std::string("org.chromium-") + base::NumberToString(pid),
      "com.example.mock_data_source", perfetto_service(),
      wait_for_registration.QuitClosure(), wait_for_start.QuitClosure(),
      kNumPackets);
  wait_for_registration.Run();

  // Start a tracing session using the client API.
  auto session = perfetto::Tracing::NewTrace();
  perfetto::TraceConfig perfetto_config;
  perfetto_config.add_buffers()->set_size_kb(1024);
  auto* ds_cfg = perfetto_config.add_data_sources()->mutable_config();
  ds_cfg->set_name("com.example.mock_data_source");
  session->Setup(perfetto_config);
  session->Start();
  wait_for_start.Run();

  // Stop the session and wait for it to stop. Note that we can't use the
  // blocking API here because the service runs on the current sequence.
  base::RunLoop wait_for_stop_loop;
  session->SetOnStopCallback(
      [&wait_for_stop_loop] { wait_for_stop_loop.Quit(); });
  session->Stop();
  wait_for_stop_loop.Run();

  // Verify tracing session statistics.
  base::RunLoop wait_for_stats_loop;
  perfetto::protos::TraceStats stats;
  auto stats_callback =
      [&wait_for_stats_loop,
       &stats](perfetto::TracingSession::GetTraceStatsCallbackArgs args) {
        EXPECT_TRUE(args.success);
        EXPECT_TRUE(stats.ParseFromArray(args.trace_stats_data.data(),
                                         args.trace_stats_data.size()));
        wait_for_stats_loop.Quit();
      };
  session->GetTraceStats(std::move(stats_callback));
  wait_for_stats_loop.Run();
  EXPECT_EQ(1024u * 1024u, stats.buffer_stats(0).buffer_size());
  EXPECT_GT(stats.buffer_stats(0).bytes_written(), 0u);
  EXPECT_EQ(0u, stats.buffer_stats(0).trace_writer_packet_loss());

  // Read and verify the data.
  EXPECT_EQ(kNumPackets, ReadAndCountTestPackets(*session));
}

}  // namespace tracing
