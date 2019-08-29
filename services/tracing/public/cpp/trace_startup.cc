// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/tracing/public/cpp/trace_startup.h"

#include "base/command_line.h"
#include "base/logging.h"
#include "base/task/thread_pool/thread_pool_instance.h"
#include "base/trace_event/trace_log.h"
#include "components/tracing/common/trace_startup_config.h"
#include "components/tracing/common/trace_to_console.h"
#include "components/tracing/common/tracing_switches.h"
#include "services/tracing/public/cpp/perfetto/trace_event_data_source.h"
#include "services/tracing/public/cpp/stack_sampling/tracing_sampler_profiler.h"
#include "services/tracing/public/cpp/tracing_features.h"

namespace tracing {
namespace {
using base::trace_event::TraceConfig;
using base::trace_event::TraceLog;
}  // namespace

bool g_tracing_initialized_after_threadpool_and_featurelist = false;

bool IsTracingInitialized() {
  return g_tracing_initialized_after_threadpool_and_featurelist;
}

void EnableStartupTracingIfNeeded() {
  const base::CommandLine& command_line =
      *base::CommandLine::ForCurrentProcess();

  // TODO(oysteine): Support startup tracing to a perfetto protobuf trace. This
  // should also enable TraceLog and call
  // TraceEventDataSource::SetupStartupTracing().
  if (command_line.HasSwitch(switches::kPerfettoOutputFile))
    return;

  // Ensure TraceLog is initialized first.
  // https://crbug.com/764357
  auto* trace_log = TraceLog::GetInstance();
  auto* startup_config = TraceStartupConfig::GetInstance();

  if (startup_config->IsEnabled()) {
    const TraceConfig& trace_config = startup_config->GetTraceConfig();
    if (TracingUsesPerfettoBackend()) {
      if (trace_config.IsCategoryGroupEnabled(
              TRACE_DISABLED_BY_DEFAULT("cpu_profiler"))) {
        TracingSamplerProfiler::SetupStartupTracing();
      }
      TraceEventDataSource::GetInstance()->SetupStartupTracing(
          startup_config->GetSessionOwner() ==
          TraceStartupConfig::SessionOwner::kBackgroundTracing);
    }

    uint8_t modes = TraceLog::RECORDING_MODE;
    if (!trace_config.event_filters().empty())
      modes |= TraceLog::FILTERING_MODE;
    trace_log->SetEnabled(startup_config->GetTraceConfig(), modes);
  } else if (command_line.HasSwitch(switches::kTraceToConsole)) {
    // TODO(eseckler): Remove ability to trace to the console, perfetto doesn't
    // support this and noone seems to use it.
    TraceConfig trace_config = GetConfigForTraceToConsole();
    LOG(ERROR) << "Start " << switches::kTraceToConsole
               << " with CategoryFilter '"
               << trace_config.ToCategoryFilterString() << "'.";
    if (TracingUsesPerfettoBackend())
      TraceEventDataSource::GetInstance()->SetupStartupTracing(
          /*privacy_filtering_enabled=*/false);
    trace_log->SetEnabled(trace_config, TraceLog::RECORDING_MODE);
  }
}

void InitTracingPostThreadPoolStartAndFeatureList() {
  if (g_tracing_initialized_after_threadpool_and_featurelist) {
    return;
  }
  g_tracing_initialized_after_threadpool_and_featurelist = true;
  // TODO(nuskos): We should switch these to DCHECK once we're reasonably
  // confident we've ensured this is called properly in all processes. Probably
  // after M78 release has been cut (since we'll verify in the rollout of M78).
  CHECK(base::ThreadPoolInstance::Get());
  CHECK(base::FeatureList::GetInstance());
  // Below are the things tracing must do once per process.
  TraceEventDataSource::GetInstance()->OnTaskSchedulerAvailable();
  if (base::FeatureList::IsEnabled(features::kEnablePerfettoSystemTracing)) {
    // To ensure System tracing connects we have to initialize the process wide
    // state. This Get() call ensures that the constructor has run.
    PerfettoTracedProcess::Get();
  }
}

}  // namespace tracing
