// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/tracing/public/cpp/traced_process_impl.h"

#include <utility>

#include "base/bind.h"
#include "base/no_destructor.h"
#include "base/task/thread_pool/thread_pool_instance.h"
#include "services/tracing/public/cpp/base_agent.h"
#include "services/tracing/public/cpp/perfetto/producer_client.h"
#include "services/tracing/public/cpp/trace_event_agent.h"
#include "services/tracing/public/mojom/constants.mojom.h"

namespace tracing {

// static
TracedProcessImpl* TracedProcessImpl::GetInstance() {
  static base::NoDestructor<TracedProcessImpl> traced_process;
  return traced_process.get();
}

TracedProcessImpl::TracedProcessImpl() : binding_(this) {
  DETACH_FROM_SEQUENCE(sequence_checker_);
}

TracedProcessImpl::~TracedProcessImpl() = default;

// OnTracedProcessRequest can be called concurrently from
// multiple threads, as we get one call per service.
void TracedProcessImpl::OnTracedProcessRequest(
    mojom::TracedProcessRequest request) {
  if (task_runner_ && !task_runner_->RunsTasksInCurrentSequence()) {
    task_runner_->PostTask(
        FROM_HERE, base::BindOnce(&TracedProcessImpl::OnTracedProcessRequest,
                                  base::Unretained(this), std::move(request)));
    return;
  }

  // We only need one binding per process.
  base::AutoLock lock(lock_);
  if (binding_.is_bound()) {
    return;
  }

  DETACH_FROM_SEQUENCE(sequence_checker_);
  binding_.Bind(std::move(request));
}

// SetTaskRunner must be called before we start receiving
// any OnTracedProcessRequest calls.
void TracedProcessImpl::SetTaskRunner(
    scoped_refptr<base::SequencedTaskRunner> task_runner) {
  DCHECK(!binding_.is_bound());
  DCHECK(!task_runner_);
  task_runner_ = task_runner;
}

void TracedProcessImpl::RegisterAgent(BaseAgent* agent) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (agent_registry_) {
    agent->Connect(agent_registry_.get());
  }

  agents_.insert(agent);
}

void TracedProcessImpl::UnregisterAgent(BaseAgent* agent) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  agents_.erase(agent);
}

void TracedProcessImpl::ConnectToTracingService(
    mojom::ConnectToTracingRequestPtr request,
    ConnectToTracingServiceCallback callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  // Acknowledge this message so the tracing service knows it was dispatched in
  // this process.
  std::move(callback).Run();

  // Tracing requires a running ThreadPool; disable tracing
  // for processes without it.
  if (!base::ThreadPoolInstance::Get()) {
    return;
  }

  // Ensure the TraceEventAgent has been created.
  TraceEventAgent::GetInstance();

  agent_registry_ = mojo::Remote<tracing::mojom::AgentRegistry>(
      std::move(request->agent_registry));
  agent_registry_.set_disconnect_handler(base::BindRepeating(
      [](TracedProcessImpl* traced_process) {
        // If the AgentRegistry connection closes, the tracing service
        // has gone down and we'll start accepting new connections from it
        // again.
        base::AutoLock lock(traced_process->lock_);
        traced_process->agent_registry_.reset();
        traced_process->binding_.Close();
      },
      base::Unretained(this)));

  for (auto* agent : agents_) {
    agent->Connect(agent_registry_.get());
  }

  PerfettoTracedProcess::Get()->producer_client()->Connect(
      mojo::PendingRemote<tracing::mojom::PerfettoService>(
          std::move(request->perfetto_service)));
}

void TracedProcessImpl::GetCategories(std::set<std::string>* category_set) {
  for (auto* agent : agents_) {
    agent->GetCategories(category_set);
  }
}

}  // namespace tracing
