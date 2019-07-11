// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_DEVTOOLS_DEVTOOLS_RENDERER_CHANNEL_H_
#define CONTENT_BROWSER_DEVTOOLS_DEVTOOLS_RENDERER_CHANNEL_H_

#include "base/callback.h"
#include "base/containers/flat_set.h"
#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "content/common/content_export.h"
#include "mojo/public/cpp/bindings/associated_binding.h"
#include "mojo/public/cpp/bindings/binding.h"
#include "third_party/blink/public/mojom/devtools/devtools_agent.mojom.h"

namespace gfx {
class Point;
}

namespace content {

class DevToolsAgentHostImpl;
class DevToolsSession;
class RenderFrameHostImpl;
class WorkerDevToolsAgentHost;

namespace protocol {
class TargetAutoAttacher;
}

// This class encapsulates a connection to blink::mojom::DevToolsAgent
// in the renderer (either RenderFrame or some kind of worker).
// When the renderer changes (e.g. worker restarts or a new RenderFrame
// is used for the frame), different DevToolsAgentHostImpl subclasses
// retrieve a new blink::mojom::DevToolsAgent pointer, and this channel
// starts using it for all existing and future sessions.
class CONTENT_EXPORT DevToolsRendererChannel
    : public blink::mojom::DevToolsAgentHost {
 public:
  explicit DevToolsRendererChannel(DevToolsAgentHostImpl* owner);
  ~DevToolsRendererChannel() override;

  // Dedicated workers use non-associated version,
  // while frames and other workers use DevToolsAgent associated
  // with respective control interfraces. See mojom for details.
  void SetRenderer(blink::mojom::DevToolsAgentPtr agent_ptr,
                   blink::mojom::DevToolsAgentHostRequest host_request,
                   int process_id,
                   RenderFrameHostImpl* frame_host);
  void SetRendererAssociated(
      blink::mojom::DevToolsAgentAssociatedPtr agent_ptr,
      blink::mojom::DevToolsAgentHostAssociatedRequest host_request,
      int process_id,
      RenderFrameHostImpl* frame_host);
  void AttachSession(DevToolsSession* session);
  void InspectElement(const gfx::Point& point);
  void ForceDetachWorkerSessions();

  void SetReportChildWorkers(protocol::TargetAutoAttacher* attacher,
                             bool report,
                             bool wait_for_debugger,
                             base::OnceClosure callback);

 private:
  // blink::mojom::DevToolsAgentHost implementation.
  void ChildWorkerCreated(blink::mojom::DevToolsAgentPtr worker_devtools_agent,
                          blink::mojom::DevToolsAgentHostRequest host_request,
                          const GURL& url,
                          const std::string& name,
                          const base::UnguessableToken& devtools_worker_token,
                          bool waiting_for_debugger) override;
  void ChildWorkerDestroyed(DevToolsAgentHostImpl*);

  void CleanupConnection();
  void SetRendererInternal(blink::mojom::DevToolsAgent* agent,
                           int process_id,
                           RenderFrameHostImpl* frame_host);
  void ReportChildWorkersCallback();

  DevToolsAgentHostImpl* owner_;
  mojo::Binding<blink::mojom::DevToolsAgentHost> binding_;
  mojo::AssociatedBinding<blink::mojom::DevToolsAgentHost> associated_binding_;
  blink::mojom::DevToolsAgentPtr agent_ptr_;
  blink::mojom::DevToolsAgentAssociatedPtr associated_agent_ptr_;
  int process_id_;
  RenderFrameHostImpl* frame_host_ = nullptr;
  base::flat_set<protocol::TargetAutoAttacher*> report_attachers_;
  base::flat_set<protocol::TargetAutoAttacher*> wait_for_debugger_attachers_;
  base::flat_set<WorkerDevToolsAgentHost*> child_workers_;
  base::OnceClosure set_report_callback_;
  base::WeakPtrFactory<DevToolsRendererChannel> weak_factory_{this};

  DISALLOW_COPY_AND_ASSIGN(DevToolsRendererChannel);
};

}  // namespace content

#endif  // CONTENT_BROWSER_DEVTOOLS_DEVTOOLS_RENDERER_CHANNEL_H_
