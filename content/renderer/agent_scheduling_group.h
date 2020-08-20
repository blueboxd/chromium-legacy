// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_RENDERER_AGENT_SCHEDULING_GROUP_H_
#define CONTENT_RENDERER_AGENT_SCHEDULING_GROUP_H_

#include <memory>

#include "content/common/agent_scheduling_group.mojom.h"
#include "content/common/content_export.h"
#include "mojo/public/cpp/bindings/remote.h"

namespace content {

// Renderer-side representation of AgentSchedulingGroup, used for communication
// with the (browser-side) AgentSchedulingGroupHost. AgentSchedulingGroup is
// Blink's unit of scheduling and performance isolation, which is the only way
// to obtain ordering guarantees between different Mojo (associated) interfaces
// and legacy IPC messages.
class CONTENT_EXPORT AgentSchedulingGroup {
 public:
  AgentSchedulingGroup();
  ~AgentSchedulingGroup();

  AgentSchedulingGroup(const AgentSchedulingGroup&) = delete;
  AgentSchedulingGroup(const AgentSchedulingGroup&&) = delete;
  AgentSchedulingGroup& operator=(const AgentSchedulingGroup&) = delete;
  AgentSchedulingGroup& operator=(const AgentSchedulingGroup&&) = delete;

 private:
  // Internal implementation of content::mojom::AgentSchedulingGroup, used for
  // responding to calls from the (browser-side) AgentSchedulingGroupHost.
  std::unique_ptr<content::mojom::AgentSchedulingGroup> mojo_impl_;

  // Remote stub of content::mojom::AgentSchedulingGroupHost, used for sending
  // calls to the (browser-side) AgentSchedulingGroupHost.
  std::unique_ptr<mojo::Remote<content::mojom::AgentSchedulingGroupHost>>
      mojo_remote_;
};

}  // namespace content

#endif
