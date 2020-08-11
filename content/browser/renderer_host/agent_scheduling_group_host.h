// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_RENDERER_HOST_AGENT_SCHEDULING_GROUP_HOST_H_
#define CONTENT_BROWSER_RENDERER_HOST_AGENT_SCHEDULING_GROUP_HOST_H_

#include "content/common/content_export.h"

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
  virtual ~AgentSchedulingGroupHost();

  RenderProcessHost* GetProcess();

 private:
  RenderProcessHost& process_;
};

}  // namespace content

#endif
