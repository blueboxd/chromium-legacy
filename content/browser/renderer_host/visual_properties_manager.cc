// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/renderer_host/visual_properties_manager.h"
#include "content/browser/renderer_host/render_view_host_impl.h"
#include "content/common/view_messages.h"
#include "content/common/visual_properties.h"

namespace content {

VisualPropertiesManager::VisualPropertiesManager(
    RenderViewHostImpl* render_view_host)
    : render_view_host_(render_view_host) {}
VisualPropertiesManager::~VisualPropertiesManager() = default;

void VisualPropertiesManager::SendVisualProperties(
    const VisualProperties& visual_properties,
    int widget_routing_id) {
  render_view_host_->Send(new ViewMsg_UpdateVisualProperties(
      render_view_host_->GetRoutingID(), visual_properties, widget_routing_id));
}

}  // namespace content
