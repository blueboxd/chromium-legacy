// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "weblayer/renderer/weblayer_render_frame_observer.h"

#include "content/public/renderer/render_frame.h"

namespace weblayer {

WebLayerRenderFrameObserver::WebLayerRenderFrameObserver(
    content::RenderFrame* render_frame)
    : content::RenderFrameObserver(render_frame) {}

WebLayerRenderFrameObserver::~WebLayerRenderFrameObserver() = default;

bool WebLayerRenderFrameObserver::OnAssociatedInterfaceRequestForFrame(
    const std::string& interface_name,
    mojo::ScopedInterfaceEndpointHandle* handle) {
  return associated_interfaces_.TryBindInterface(interface_name, handle);
}

void WebLayerRenderFrameObserver::OnDestruct() {
  delete this;
}

}  // namespace weblayer
