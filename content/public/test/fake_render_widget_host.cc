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

std::pair<mojo::PendingAssociatedRemote<blink::mojom::WidgetHost>,
          mojo::PendingAssociatedReceiver<blink::mojom::Widget>>
FakeRenderWidgetHost::BindNewWidgetInterfaces() {
  widget_host_receiver_.reset();
  widget_remote_.reset();
  return std::make_pair(
      widget_host_receiver_.BindNewEndpointAndPassDedicatedRemoteForTesting(),
      widget_remote_.BindNewEndpointAndPassDedicatedReceiverForTesting());
}

void FakeRenderWidgetHost::AnimateDoubleTapZoomInMainFrame(
    const gfx::Point& tap_point,
    const gfx::Rect& rect_to_zoom) {}

void FakeRenderWidgetHost::ZoomToFindInPageRectInMainFrame(
    const gfx::Rect& rect_to_zoom) {}

void FakeRenderWidgetHost::SetHasTouchEventHandlers(bool has_handlers) {}

void FakeRenderWidgetHost::IntrinsicSizingInfoChanged(
    blink::mojom::IntrinsicSizingInfoPtr sizing_info) {}

void FakeRenderWidgetHost::SetCursor(const ui::Cursor& cursor) {}

void FakeRenderWidgetHost::SetToolTipText(
    const base::string16& tooltip_text,
    base::i18n::TextDirection text_direction_hint) {}

void FakeRenderWidgetHost::AutoscrollStart(const gfx::PointF& position) {}

void FakeRenderWidgetHost::AutoscrollFling(const gfx::Vector2dF& position) {}

void FakeRenderWidgetHost::AutoscrollEnd() {}

}  // namespace content
