// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/public/test/fake_render_widget_host.h"

#include "third_party/blink/public/mojom/frame/intrinsic_sizing_info.mojom.h"

namespace content {

FakeRenderWidgetHost::FakeRenderWidgetHost() = default;
FakeRenderWidgetHost::~FakeRenderWidgetHost() = default;

std::pair<mojo::PendingAssociatedRemote<blink::mojom::FrameWidgetHost>,
          mojo::PendingAssociatedReceiver<blink::mojom::FrameWidget>>
FakeRenderWidgetHost::BindNewFrameWidgetInterfaces() {
  frame_widget_host_receiver_.reset();
  frame_widget_remote_.reset();
  return std::make_pair(
      frame_widget_host_receiver_
          .BindNewEndpointAndPassDedicatedRemoteForTesting(),
      frame_widget_remote_.BindNewEndpointAndPassDedicatedReceiverForTesting());
}

void FakeRenderWidgetHost::AnimateDoubleTapZoomInMainFrame(
    const gfx::Point& tap_point,
    const gfx::Rect& rect_to_zoom) {}

void FakeRenderWidgetHost::ZoomToFindInPageRectInMainFrame(
    const gfx::Rect& rect_to_zoom) {}

void FakeRenderWidgetHost::SetHasTouchEventHandlers(bool has_handlers) {}

void FakeRenderWidgetHost::IntrinsicSizingInfoChanged(
    blink::mojom::IntrinsicSizingInfoPtr sizing_info) {}

}  // namespace content
