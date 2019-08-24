// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/resource_coordinator/public/cpp/memory_instrumentation/memory_instrumentation.h"

#include "base/bind.h"
#include "base/single_thread_task_runner.h"
#include "base/threading/thread_task_runner_handle.h"

namespace memory_instrumentation {
namespace {
MemoryInstrumentation* g_instance = nullptr;

void WrapGlobalMemoryDump(
    MemoryInstrumentation::RequestGlobalDumpCallback callback,
    bool success,
    mojom::GlobalMemoryDumpPtr dump) {
  std::move(callback).Run(success, GlobalMemoryDump::MoveFrom(std::move(dump)));
}
}  // namespace

// static
void MemoryInstrumentation::CreateInstance(
    service_manager::Connector* connector,
    const std::string& service_name) {
  DCHECK(!g_instance);
  g_instance = new MemoryInstrumentation(connector, service_name);
}

// static
MemoryInstrumentation* MemoryInstrumentation::GetInstance() {
  return g_instance;
}

MemoryInstrumentation::MemoryInstrumentation(
    service_manager::Connector* connector,
    const std::string& service_name)
    : connector_(connector),
      connector_task_runner_(base::ThreadTaskRunnerHandle::Get()),
      service_name_(service_name) {
  DCHECK(connector_task_runner_);
}

MemoryInstrumentation::~MemoryInstrumentation() {
  g_instance = nullptr;
}

void MemoryInstrumentation::RequestGlobalDump(
    const std::vector<std::string>& allocator_dump_names,
    RequestGlobalDumpCallback callback) {
  const auto& coordinator = GetCoordinatorBindingForCurrentSequence();
  coordinator->RequestGlobalMemoryDump(
      MemoryDumpType::SUMMARY_ONLY, MemoryDumpLevelOfDetail::BACKGROUND,
      allocator_dump_names,
      base::BindOnce(&WrapGlobalMemoryDump, std::move(callback)));
}

void MemoryInstrumentation::RequestPrivateMemoryFootprint(
    base::ProcessId pid,
    RequestGlobalDumpCallback callback) {
  const auto& coordinator = GetCoordinatorBindingForCurrentSequence();
  coordinator->RequestPrivateMemoryFootprint(
      pid, base::BindOnce(&WrapGlobalMemoryDump, std::move(callback)));
}

void MemoryInstrumentation::RequestGlobalDumpForPid(
    base::ProcessId pid,
    const std::vector<std::string>& allocator_dump_names,
    RequestGlobalDumpCallback callback) {
  const auto& coordinator = GetCoordinatorBindingForCurrentSequence();
  coordinator->RequestGlobalMemoryDumpForPid(
      pid, allocator_dump_names,
      base::BindOnce(&WrapGlobalMemoryDump, std::move(callback)));
}

void MemoryInstrumentation::RequestGlobalDumpAndAppendToTrace(
    MemoryDumpType dump_type,
    MemoryDumpLevelOfDetail level_of_detail,
    RequestGlobalMemoryDumpAndAppendToTraceCallback callback) {
  const auto& coordinator = GetCoordinatorBindingForCurrentSequence();
  coordinator->RequestGlobalMemoryDumpAndAppendToTrace(
      dump_type, level_of_detail, std::move(callback));
}

const mojom::CoordinatorPtr&
MemoryInstrumentation::GetCoordinatorBindingForCurrentSequence() {
  auto* coordinator = sequence_storage_coordinator_.GetValuePointer();
  if (!coordinator) {
    coordinator = sequence_storage_coordinator_.emplace();
    mojom::CoordinatorRequest coordinator_req = mojo::MakeRequest(coordinator);

    // The connector is not thread safe and BindInterface must be called on its
    // own thread. Thankfully, the binding can happen _after_ having started
    // invoking methods on the |coordinator| proxy objects.
    connector_task_runner_->PostTask(
        FROM_HERE,
        base::BindOnce(
            &MemoryInstrumentation::BindCoordinatorRequestOnConnectorThread,
            base::Unretained(this), std::move(coordinator_req)));
  }
  return *coordinator;
}

void MemoryInstrumentation::BindCoordinatorRequestOnConnectorThread(
    mojom::CoordinatorRequest coordinator_request) {
  connector_->BindInterface(service_name_, std::move(coordinator_request));
}

}  // namespace memory_instrumentation
