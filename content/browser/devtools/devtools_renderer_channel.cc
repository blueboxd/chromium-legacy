// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/devtools/devtools_renderer_channel.h"

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "content/browser/devtools/devtools_agent_host_impl.h"
#include "content/browser/devtools/devtools_session.h"
#include "content/browser/devtools/protocol/devtools_domain_handler.h"
#include "content/browser/devtools/protocol/target_auto_attacher.h"
#include "content/browser/devtools/worker_devtools_agent_host.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/common/child_process_host.h"
#include "ui/gfx/geometry/point.h"

namespace content {

DevToolsRendererChannel::DevToolsRendererChannel(DevToolsAgentHostImpl* owner)
    : owner_(owner),
      binding_(this),
      associated_binding_(this),
      process_id_(ChildProcessHost::kInvalidUniqueID) {}

DevToolsRendererChannel::~DevToolsRendererChannel() = default;

void DevToolsRendererChannel::SetRenderer(
    blink::mojom::DevToolsAgentPtr agent_ptr,
    blink::mojom::DevToolsAgentHostRequest host_request,
    int process_id,
    RenderFrameHostImpl* frame_host) {
  CleanupConnection();
  agent_ptr_ = std::move(agent_ptr);
  if (host_request)
    binding_.Bind(std::move(host_request));
  SetRendererInternal(agent_ptr_.get(), process_id, frame_host);
}

void DevToolsRendererChannel::SetRendererAssociated(
    blink::mojom::DevToolsAgentAssociatedPtr agent_ptr,
    blink::mojom::DevToolsAgentHostAssociatedRequest host_request,
    int process_id,
    RenderFrameHostImpl* frame_host) {
  CleanupConnection();
  associated_agent_ptr_ = std::move(agent_ptr);
  if (host_request)
    associated_binding_.Bind(std::move(host_request));
  SetRendererInternal(associated_agent_ptr_.get(), process_id, frame_host);
}

void DevToolsRendererChannel::CleanupConnection() {
  binding_.Close();
  associated_binding_.Close();
  associated_agent_ptr_ = nullptr;
  agent_ptr_ = nullptr;
}

void DevToolsRendererChannel::ForceDetachWorkerSessions() {
  for (WorkerDevToolsAgentHost* host : child_workers_)
    host->ForceDetachAllSessions();
}

void DevToolsRendererChannel::SetRendererInternal(
    blink::mojom::DevToolsAgent* agent,
    int process_id,
    RenderFrameHostImpl* frame_host) {
  ReportChildWorkersCallback();
  process_id_ = process_id;
  frame_host_ = frame_host;
  if (agent && !report_attachers_.empty()) {
    agent->ReportChildWorkers(true /* report */,
                              !wait_for_debugger_attachers_.empty(),
                              base::DoNothing());
  }
  for (DevToolsSession* session : owner_->sessions()) {
    for (auto& pair : session->handlers())
      pair.second->SetRenderer(process_id_, frame_host_);
    session->AttachToAgent(agent);
  }
}

void DevToolsRendererChannel::AttachSession(DevToolsSession* session) {
  if (!agent_ptr_ && !associated_agent_ptr_)
    owner_->UpdateRendererChannel(true /* force */);
  for (auto& pair : session->handlers())
    pair.second->SetRenderer(process_id_, frame_host_);
  if (agent_ptr_)
    session->AttachToAgent(agent_ptr_.get());
  else if (associated_agent_ptr_)
    session->AttachToAgent(associated_agent_ptr_.get());
}

void DevToolsRendererChannel::InspectElement(const gfx::Point& point) {
  if (!agent_ptr_ && !associated_agent_ptr_)
    owner_->UpdateRendererChannel(true /* force */);
  // Previous call might update |agent_ptr_| or |associated_agent_ptr_|
  // via SetRenderer(), so we should check them again.
  if (agent_ptr_)
    agent_ptr_->InspectElement(point);
  else if (associated_agent_ptr_)
    associated_agent_ptr_->InspectElement(point);
}

void DevToolsRendererChannel::SetReportChildWorkers(
    protocol::TargetAutoAttacher* attacher,
    bool report,
    bool wait_for_debugger,
    base::OnceClosure callback) {
  ReportChildWorkersCallback();
  set_report_callback_ = std::move(callback);
  if (report) {
    if (report_attachers_.find(attacher) == report_attachers_.end()) {
      report_attachers_.insert(attacher);
      for (DevToolsAgentHostImpl* host : child_workers_)
        attacher->ChildWorkerCreated(host, false /* waiting_for_debugger */);
    }
  } else {
    report_attachers_.erase(attacher);
  }
  if (wait_for_debugger)
    wait_for_debugger_attachers_.insert(attacher);
  else
    wait_for_debugger_attachers_.erase(attacher);
  if (agent_ptr_) {
    agent_ptr_->ReportChildWorkers(
        !report_attachers_.empty(), !wait_for_debugger_attachers_.empty(),
        base::BindOnce(&DevToolsRendererChannel::ReportChildWorkersCallback,
                       base::Unretained(this)));
  } else if (associated_agent_ptr_) {
    associated_agent_ptr_->ReportChildWorkers(
        !report_attachers_.empty(), !wait_for_debugger_attachers_.empty(),
        base::BindOnce(&DevToolsRendererChannel::ReportChildWorkersCallback,
                       base::Unretained(this)));
  } else {
    ReportChildWorkersCallback();
  }
}

void DevToolsRendererChannel::ReportChildWorkersCallback() {
  if (set_report_callback_)
    std::move(set_report_callback_).Run();
}

void DevToolsRendererChannel::ChildWorkerCreated(
    blink::mojom::DevToolsAgentPtr worker_devtools_agent,
    blink::mojom::DevToolsAgentHostRequest host_request,
    const GURL& url,
    const std::string& name,
    const base::UnguessableToken& devtools_worker_token,
    bool waiting_for_debugger) {
  if (content::DevToolsAgentHost::GetForId(devtools_worker_token.ToString())) {
    mojo::ReportBadMessage("Workers should have unique tokens.");
    return;
  }
  RenderProcessHost* process = RenderProcessHost::FromID(process_id_);
  if (!process)
    return;
  GURL filtered_url = url;
  process->FilterURL(true /* empty_allowed */, &filtered_url);
  auto agent_host = base::MakeRefCounted<WorkerDevToolsAgentHost>(
      process_id_, std::move(worker_devtools_agent), std::move(host_request),
      filtered_url, std::move(name), devtools_worker_token, owner_->GetId(),
      base::BindOnce(&DevToolsRendererChannel::ChildWorkerDestroyed,
                     weak_factory_.GetWeakPtr()));
  child_workers_.insert(agent_host.get());
  for (protocol::TargetAutoAttacher* attacher : report_attachers_)
    attacher->ChildWorkerCreated(agent_host.get(), waiting_for_debugger);
}

void DevToolsRendererChannel::ChildWorkerDestroyed(
    DevToolsAgentHostImpl* host) {
  child_workers_.erase(host);
}

}  // namespace content
