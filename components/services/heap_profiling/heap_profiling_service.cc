// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/services/heap_profiling/heap_profiling_service.h"

#include <memory>
#include <utility>

#include "base/bind.h"
#include "base/logging.h"
#include "base/task/post_task.h"
#include "components/services/heap_profiling/public/mojom/heap_profiling_client.mojom.h"
#include "mojo/public/cpp/system/platform_handle.h"
#include "services/resource_coordinator/public/cpp/resource_coordinator_features.h"
#include "services/resource_coordinator/public/mojom/memory_instrumentation/memory_instrumentation.mojom.h"
#include "services/resource_coordinator/public/mojom/service_constants.mojom.h"

namespace heap_profiling {

HeapProfilingService::HeapProfilingService(
    service_manager::mojom::ServiceRequest request)
    : service_binding_(this, std::move(request)) {}

HeapProfilingService::~HeapProfilingService() = default;

// static
base::RepeatingCallback<void(service_manager::mojom::ServiceRequest)>
HeapProfilingService::GetServiceFactory() {
  return base::BindRepeating(
      [](service_manager::mojom::ServiceRequest request) {
        // base::WithBaseSyncPrimitives() and thus DEDICATED are needed
        // because the thread owned by ConnectionManager::Connection is doing
        // blocking Join during dectruction.
        scoped_refptr<base::SingleThreadTaskRunner> task_runner =
            base::CreateSingleThreadTaskRunner(
                {base::ThreadPool(), base::TaskPriority::BEST_EFFORT,
                 base::WithBaseSyncPrimitives()},
                base::SingleThreadTaskRunnerThreadMode::DEDICATED);
        task_runner->PostTask(
            FROM_HERE,
            base::BindOnce(
                [](service_manager::mojom::ServiceRequest request) {
                  service_manager::Service::RunAsyncUntilTermination(
                      std::make_unique<heap_profiling::HeapProfilingService>(
                          std::move(request)));
                },
                std::move(request)));
      });
}

void HeapProfilingService::OnStart() {
  registry_.AddInterface(
      base::Bind(&HeapProfilingService::OnProfilingServiceRequest,
                 base::Unretained(this)));
  registry_.AddInterface(base::Bind(
      &HeapProfilingService::OnHeapProfilerRequest, base::Unretained(this)));
}

void HeapProfilingService::OnBindInterface(
    const service_manager::BindSourceInfo& source_info,
    const std::string& interface_name,
    mojo::ScopedMessagePipeHandle interface_pipe) {
  registry_.BindInterface(interface_name, std::move(interface_pipe));
}

void HeapProfilingService::OnProfilingServiceRequest(
    mojom::ProfilingServiceRequest request) {
  binding_.Bind(std::move(request));
}

void HeapProfilingService::OnHeapProfilerRequest(
    memory_instrumentation::mojom::HeapProfilerRequest request) {
  heap_profiler_binding_.Bind(std::move(request));
}

void HeapProfilingService::AddProfilingClient(
    base::ProcessId pid,
    mojom::ProfilingClientPtr client,
    mojom::ProcessType process_type,
    mojom::ProfilingParamsPtr params) {
  if (params->sampling_rate == 0)
    params->sampling_rate = 1;
  connection_manager_.OnNewConnection(pid, std::move(client), process_type,
                                      std::move(params));
}

void HeapProfilingService::GetProfiledPids(GetProfiledPidsCallback callback) {
  std::move(callback).Run(connection_manager_.GetConnectionPids());
}

void HeapProfilingService::DumpProcessesForTracing(
    bool strip_path_from_mapped_files,
    DumpProcessesForTracingCallback callback) {
  std::vector<base::ProcessId> pids =
      connection_manager_.GetConnectionPidsThatNeedVmRegions();
  if (pids.empty()) {
    connection_manager_.DumpProcessesForTracing(
        strip_path_from_mapped_files, std::move(callback), VmRegions());
    return;
  }

  // Need a memory map to make sense of the dump. The dump will be triggered
  // in the memory map global dump callback.
  if (!helper_) {
    service_binding_.GetConnector()->BindInterface(
        resource_coordinator::mojom::kServiceName, &helper_);
  }
  helper_->GetVmRegionsForHeapProfiler(
      pids, base::BindOnce(&HeapProfilingService::
                               OnGetVmRegionsCompleteForDumpProcessesForTracing,
                           weak_factory_.GetWeakPtr(),
                           strip_path_from_mapped_files, std::move(callback)));
}

void HeapProfilingService::OnGetVmRegionsCompleteForDumpProcessesForTracing(
    bool strip_path_from_mapped_files,
    DumpProcessesForTracingCallback callback,
    VmRegions vm_regions) {
  connection_manager_.DumpProcessesForTracing(
      strip_path_from_mapped_files, std::move(callback), std::move(vm_regions));
}

}  // namespace heap_profiling
