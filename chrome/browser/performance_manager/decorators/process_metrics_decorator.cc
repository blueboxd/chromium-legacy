// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/performance_manager/decorators/process_metrics_decorator.h"

#include "base/feature_list.h"
#include "build/build_config.h"
#include "chrome/browser/performance_manager/policies/policy_features.h"
#include "components/performance_manager/graph/graph_impl.h"
#include "components/performance_manager/graph/node_attached_data_impl.h"
#include "components/performance_manager/graph/process_node_impl.h"
#include "components/performance_manager/graph/system_node_impl.h"
#include "services/resource_coordinator/public/cpp/memory_instrumentation/global_memory_dump.h"

namespace performance_manager {


namespace {

// The default process metrics refresh interval.
constexpr base::TimeDelta kDefaultRefreshTimerPeriod =
    base::TimeDelta::FromMinutes(2);

#if !defined(OS_ANDROID)
// The fast process metrics refresh interval. Used in certain situations, see
// the comment in ProcessMetricsDecorator::StartTimer for more details.
constexpr base::TimeDelta kFastRefreshTimerPeriod =
    base::TimeDelta::FromSeconds(20);
#endif

}  // namespace

ProcessMetricsDecorator::ProcessMetricsDecorator() = default;
ProcessMetricsDecorator::~ProcessMetricsDecorator() = default;

void ProcessMetricsDecorator::OnPassedToGraph(Graph* graph) {
  graph_ = graph;
  StartTimer();
}

void ProcessMetricsDecorator::OnTakenFromGraph(Graph* graph) {
  StopTimer();
  graph_ = nullptr;
}

void ProcessMetricsDecorator::StartTimer() {
  base::TimeDelta refresh_period = kDefaultRefreshTimerPeriod;

#if !defined(OS_ANDROID)
  // Bump the refresh frequency when urgent discarding is done from the graph or
  // when emitting memory pressure signals on high PMF as these features relies
  // on relatively fresh data.
  // TODO(sebmarchand): Measure the performance impact of this.
  if (base::FeatureList::IsEnabled(
          features::kUrgentDiscardingFromPerformanceManager) ||
      base::FeatureList::IsEnabled(features::kHighPMFMemoryPressureSignals)) {
    refresh_period = kFastRefreshTimerPeriod;
  }
#endif

  refresh_timer_.Start(
      FROM_HERE, refresh_period,
      base::BindRepeating(&ProcessMetricsDecorator::RefreshMetrics,
                          base::Unretained(this)));
}

void ProcessMetricsDecorator::StopTimer() {
  refresh_timer_.Stop();
}

void ProcessMetricsDecorator::RefreshMetrics() {
  RequestProcessesMemoryMetrics(base::BindOnce(
      &ProcessMetricsDecorator::DidGetMemoryUsage, weak_factory_.GetWeakPtr()));
}

void ProcessMetricsDecorator::RequestProcessesMemoryMetrics(
    memory_instrumentation::MemoryInstrumentation::RequestGlobalDumpCallback
        callback) {
  // TODO(sebmarchand): Use the synchronous calls once they are available.
  auto* mem_instrumentation =
      memory_instrumentation::MemoryInstrumentation::GetInstance();
  // The memory instrumentation service is not available in unit tests unless
  // explicitly created.
  if (mem_instrumentation) {
    mem_instrumentation->RequestPrivateMemoryFootprint(base::kNullProcessId,
                                                       std::move(callback));
  }
}

void ProcessMetricsDecorator::DidGetMemoryUsage(
    bool success,
    std::unique_ptr<memory_instrumentation::GlobalMemoryDump> process_dumps) {
  if (!success)
    return;

  auto* graph_impl = GraphImpl::FromGraph(graph_);

  // Refresh the process nodes with the data contained in |process_dumps|.
  // Processes for which we don't receive any data will retain the previously
  // set value.
  // TODO(sebmarchand): Check if we should set the data to 0 instead, or add a
  // timestamp to the data.
  for (const auto& process_dump_iter : process_dumps->process_dumps()) {
    // Check if there's a process node associated with this PID.
    auto* node = graph_impl->GetProcessNodeByPid(process_dump_iter.pid());
    if (!node)
      continue;

    node->set_private_footprint_kb(
        process_dump_iter.os_dump().private_footprint_kb);
    node->set_resident_set_kb(process_dump_iter.os_dump().resident_set_kb);
  }

  GraphImpl::FromGraph(graph_)
      ->FindOrCreateSystemNodeImpl()
      ->OnProcessMemoryMetricsAvailable();
  refresh_timer_.Reset();
}

}  // namespace performance_manager
