// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/performance_manager/decorators/process_metrics_decorator.h"

#include "chrome/browser/performance_manager/graph/graph_impl.h"
#include "chrome/browser/performance_manager/graph/node_attached_data_impl.h"
#include "chrome/browser/performance_manager/graph/process_node_impl.h"
#include "services/resource_coordinator/public/cpp/memory_instrumentation/global_memory_dump.h"

namespace performance_manager {

// Provides ProcessMetricsDecorator machinery access to some internals of a
// ProcessNodeImpl.
class ProcessMetricsDecoratorAccess {
 public:
  static std::unique_ptr<NodeAttachedData>* GetUniquePtrStorage(
      ProcessNodeImpl* process_node) {
    return &process_node->process_metrics_data_;
  }
};

namespace {

// The process metrics refresh interval.
constexpr base::TimeDelta kRefreshTimerPeriod = base::TimeDelta::FromMinutes(2);

// Private implementation of the node attached data. This keeps the complexity
// out of the header file.
class ProcessMetricsDataImpl
    : public ProcessMetricsDecorator::Data,
      public NodeAttachedDataImpl<ProcessMetricsDataImpl> {
 public:
  struct Traits : public NodeAttachedDataOwnedByNodeType<ProcessNodeImpl> {};

  explicit ProcessMetricsDataImpl(const ProcessNodeImpl* process_node) {}
  ~ProcessMetricsDataImpl() override = default;

  static std::unique_ptr<NodeAttachedData>* GetUniquePtrStorage(
      ProcessNodeImpl* process_node) {
    return ProcessMetricsDecoratorAccess::GetUniquePtrStorage(process_node);
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(ProcessMetricsDataImpl);
};

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
  refresh_timer_.Start(
      FROM_HERE, kRefreshTimerPeriod,
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
  auto* mem_instrumentation =
      memory_instrumentation::MemoryInstrumentation::GetInstance();
  mem_instrumentation->RequestPrivateMemoryFootprint(base::kNullProcessId,
                                                     std::move(callback));
}

void ProcessMetricsDecorator::DidGetMemoryUsage(
    bool success,
    std::unique_ptr<memory_instrumentation::GlobalMemoryDump> process_dumps) {
  if (!success)
    return;

  auto* graph_impl = GraphImpl::FromGraph(graph_);
  for (const auto& process_dump_iter : process_dumps->process_dumps()) {
    // Check if there's a process node associated with this PID.
    auto* node = graph_impl->GetProcessNodeByPid(process_dump_iter.pid());
    if (!node)
      continue;

    auto* data = ProcessMetricsDataImpl::GetOrCreate(node);
    data->private_footprint_kb_ =
        process_dump_iter.os_dump().private_footprint_kb;
    data->resident_set_kb_ = process_dump_iter.os_dump().resident_set_kb;
  }

  refresh_timer_.Reset();
}

// static
ProcessMetricsDecorator::Data* ProcessMetricsDecorator::Data::GetForTesting(
    ProcessNode* process_node) {
  return ProcessMetricsDataImpl::Get(ProcessNodeImpl::FromNode(process_node));
}

}  // namespace performance_manager
