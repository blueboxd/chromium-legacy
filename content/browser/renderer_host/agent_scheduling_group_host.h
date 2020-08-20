// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_RENDERER_HOST_AGENT_SCHEDULING_GROUP_HOST_H_
#define CONTENT_BROWSER_RENDERER_HOST_AGENT_SCHEDULING_GROUP_HOST_H_

#include <stdint.h>
#include <memory>

#include "content/common/agent_scheduling_group.mojom.h"
#include "content/common/associated_interfaces.mojom-forward.h"
#include "content/common/content_export.h"
#include "content/common/renderer.mojom-forward.h"
#include "ipc/ipc_listener.h"
#include "mojo/public/cpp/bindings/remote.h"

namespace IPC {
class ChannelProxy;
class Listener;
class Message;
}  // namespace IPC

namespace content {

class RenderProcessHost;
class SiteInstance;

// Browser-side host of an AgentSchedulingGroup, used for
// AgentSchedulingGroup-bound messaging. AgentSchedulingGroup is Blink's unit of
// scheduling and performance isolation, which is the only way to obtain
// ordering guarantees between different Mojo (associated) interfaces and legacy
// IPC messages.
//
// An AgentSchedulingGroupHost is stored as (and owned by) UserData on the
// RenderProcessHost.
class CONTENT_EXPORT AgentSchedulingGroupHost {
 public:
  // Get the appropriate AgentSchedulingGroupHost for the given |instance| and
  // |process|. For now, each RenderProcessHost has a single
  // AgentSchedulingGroupHost, though future policies will allow multiple groups
  // in a process.
  static AgentSchedulingGroupHost* Get(const SiteInstance& instance,
                                       RenderProcessHost& process);

  // Should not be called explicitly. Use Get() instead.
  explicit AgentSchedulingGroupHost(RenderProcessHost& process);
  ~AgentSchedulingGroupHost();

  RenderProcessHost* GetProcess();

  // IPC and mojo messages to be forwarded to the RenderProcessHost, for now. In
  // the future they will be handled directly by the AgentSchedulingGroupHost.
  // IPC:
  IPC::ChannelProxy* GetChannel();
  bool Send(IPC::Message* message);
  void AddRoute(int32_t routing_id, IPC::Listener* listener);
  void RemoveRoute(int32_t routing_id);

  // Mojo:
  mojom::RouteProvider* GetRemoteRouteProvider();
  void CreateFrame(mojom::CreateFrameParamsPtr params);

 private:
  // The RenderProcessHost this AgentSchedulingGroup is assigned to.
  RenderProcessHost& process_;

  // Internal implementation of content::mojom::AgentSchedulingGroupHost, used
  // for responding to calls from the (renderer-side) AgentSchedulingGroup.
  std::unique_ptr<content::mojom::AgentSchedulingGroupHost> mojo_impl_;

  // Remote stub of content::mojom::AgentSchedulingGroup, used for sending calls
  // to the (renderer-side) AgentSchedulingGroup.
  std::unique_ptr<mojo::Remote<content::mojom::AgentSchedulingGroup>>
      mojo_remote_;
};

}  // namespace content

#endif
