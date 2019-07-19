// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/inspector/devtools_agent.h"

#include <v8-inspector.h>
#include <memory>

#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/exported/web_dev_tools_agent_impl.h"
#include "third_party/blink/renderer/core/frame/local_frame.h"
#include "third_party/blink/renderer/core/frame/web_local_frame_impl.h"
#include "third_party/blink/renderer/core/inspector/devtools_session.h"
#include "third_party/blink/renderer/core/inspector/inspected_frames.h"
#include "third_party/blink/renderer/core/inspector/inspector_task_runner.h"
#include "third_party/blink/renderer/core/inspector/worker_devtools_params.h"
#include "third_party/blink/renderer/core/inspector/worker_inspector_controller.h"
#include "third_party/blink/renderer/core/probe/core_probes.h"
#include "third_party/blink/renderer/core/workers/worker_global_scope.h"
#include "third_party/blink/renderer/core/workers/worker_thread.h"
#include "third_party/blink/renderer/platform/heap/persistent.h"
#include "third_party/blink/renderer/platform/wtf/text/wtf_string.h"

namespace blink {

namespace {

DevToolsAgent* DevToolsAgentFromContext(ExecutionContext* execution_context) {
  if (!execution_context)
    return nullptr;
  if (auto* scope = DynamicTo<WorkerGlobalScope>(execution_context)) {
    return scope->GetThread()
        ->GetWorkerInspectorController()
        ->GetDevToolsAgent();
  }
  if (auto* document = DynamicTo<Document>(execution_context)) {
    LocalFrame* frame = document->GetFrame();
    if (!frame)
      return nullptr;
    WebLocalFrameImpl* web_frame =
        WebLocalFrameImpl::FromFrame(frame->LocalFrameRoot());
    if (!web_frame)
      return nullptr;
    return web_frame->DevToolsAgentImpl()->GetDevToolsAgent();
  }
  return nullptr;
}

}  // namespace

DevToolsAgent::DevToolsAgent(
    Client* client,
    InspectedFrames* inspected_frames,
    CoreProbeSink* probe_sink,
    scoped_refptr<InspectorTaskRunner> inspector_task_runner,
    scoped_refptr<base::SingleThreadTaskRunner> io_task_runner)
    : client_(client),
      binding_(this),
      associated_binding_(this),
      inspected_frames_(inspected_frames),
      probe_sink_(probe_sink),
      inspector_task_runner_(std::move(inspector_task_runner)),
      io_task_runner_(std::move(io_task_runner)) {}

DevToolsAgent::~DevToolsAgent() {}

void DevToolsAgent::Trace(blink::Visitor* visitor) {
  visitor->Trace(inspected_frames_);
  visitor->Trace(probe_sink_);
  visitor->Trace(sessions_);
}

void DevToolsAgent::Dispose() {
  HeapHashSet<Member<DevToolsSession>> copy(sessions_);
  for (auto& session : copy)
    session->Detach();
  CleanupConnection();
}

void DevToolsAgent::BindRequest(
    mojom::blink::DevToolsAgentHostPtrInfo host_ptr_info,
    mojom::blink::DevToolsAgentRequest request,
    scoped_refptr<base::SingleThreadTaskRunner> task_runner) {
  DCHECK(!binding_);
  DCHECK(!associated_binding_);
  binding_.Bind(std::move(request), std::move(task_runner));
  host_ptr_.Bind(std::move(host_ptr_info));
  host_ptr_.set_connection_error_handler(
      WTF::Bind(&DevToolsAgent::CleanupConnection, WrapWeakPersistent(this)));
}

void DevToolsAgent::BindRequest(
    mojom::blink::DevToolsAgentHostAssociatedPtrInfo host_ptr_info,
    mojom::blink::DevToolsAgentAssociatedRequest request,
    scoped_refptr<base::SingleThreadTaskRunner> task_runner) {
  DCHECK(!binding_);
  DCHECK(!associated_binding_);
  associated_binding_.Bind(std::move(request), std::move(task_runner));
  associated_host_ptr_.Bind(std::move(host_ptr_info));
  associated_host_ptr_.set_connection_error_handler(
      WTF::Bind(&DevToolsAgent::CleanupConnection, WrapWeakPersistent(this)));
}

void DevToolsAgent::AttachDevToolsSession(
    mojom::blink::DevToolsSessionHostAssociatedPtrInfo host,
    mojom::blink::DevToolsSessionAssociatedRequest session_request,
    mojom::blink::DevToolsSessionRequest io_session_request,
    mojom::blink::DevToolsSessionStatePtr reattach_session_state,
    bool client_expects_binary_responses) {
  client_->DebuggerTaskStarted();
  DevToolsSession* session = MakeGarbageCollected<DevToolsSession>(
      this, std::move(host), std::move(session_request),
      std::move(io_session_request), std::move(reattach_session_state),
      client_expects_binary_responses);
  sessions_.insert(session);
  client_->DebuggerTaskFinished();
}

void DevToolsAgent::InspectElement(const WebPoint& point) {
  client_->InspectElement(point);
}

void DevToolsAgent::FlushProtocolNotifications() {
  for (auto& session : sessions_)
    session->FlushProtocolNotifications();
}

void DevToolsAgent::ReportChildWorkers(bool report,
                                       bool wait_for_debugger,
                                       base::OnceClosure callback) {
  report_child_workers_ = report;
  pause_child_workers_on_start_ = wait_for_debugger;
  if (report_child_workers_) {
    auto workers = std::move(unreported_child_worker_threads_);
    for (auto& it : workers)
      ReportChildWorker(std::move(it.value));
  }
  std::move(callback).Run();
}

// static
std::unique_ptr<WorkerDevToolsParams> DevToolsAgent::WorkerThreadCreated(
    ExecutionContext* parent_context,
    WorkerThread* worker_thread,
    const KURL& url,
    const String& global_scope_name) {
  auto result = std::make_unique<WorkerDevToolsParams>();
  result->devtools_worker_token = base::UnguessableToken::Create();

  DevToolsAgent* agent = DevToolsAgentFromContext(parent_context);
  if (!agent)
    return result;

  auto data = std::make_unique<WorkerData>();
  data->url = url;
  result->agent_request = mojo::MakeRequest(&data->agent_ptr);
  data->host_request = mojo::MakeRequest(&result->agent_host_ptr_info);
  data->devtools_worker_token = result->devtools_worker_token;
  data->waiting_for_debugger = agent->pause_child_workers_on_start_;
  data->name = global_scope_name;
  result->wait_for_debugger = agent->pause_child_workers_on_start_;

  if (agent->report_child_workers_) {
    agent->ReportChildWorker(std::move(data));
  } else {
    agent->unreported_child_worker_threads_.insert(worker_thread,
                                                   std::move(data));
  }
  return result;
}

// static
void DevToolsAgent::WorkerThreadTerminated(ExecutionContext* parent_context,
                                           WorkerThread* worker_thread) {
  if (DevToolsAgent* agent = DevToolsAgentFromContext(parent_context))
    agent->unreported_child_worker_threads_.erase(worker_thread);
}

void DevToolsAgent::ReportChildWorker(std::unique_ptr<WorkerData> data) {
  if (host_ptr_.is_bound()) {
    host_ptr_->ChildWorkerCreated(
        std::move(data->agent_ptr), std::move(data->host_request),
        std::move(data->url), std::move(data->name),
        data->devtools_worker_token, data->waiting_for_debugger);
  } else if (associated_host_ptr_.is_bound()) {
    associated_host_ptr_->ChildWorkerCreated(
        std::move(data->agent_ptr), std::move(data->host_request),
        std::move(data->url), std::move(data->name),
        data->devtools_worker_token, data->waiting_for_debugger);
  }
}

void DevToolsAgent::CleanupConnection() {
  binding_.Close();
  associated_binding_.Close();
  host_ptr_.reset();
  associated_host_ptr_.reset();
  report_child_workers_ = false;
  pause_child_workers_on_start_ = false;
}

}  // namespace blink
