// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_PUBLIC_TEST_FAKE_RENDER_WIDGET_HOST_H_
#define CONTENT_PUBLIC_TEST_FAKE_RENDER_WIDGET_HOST_H_

#include <utility>

#include "base/macros.h"
#include "mojo/public/cpp/bindings/associated_receiver.h"
#include "mojo/public/cpp/bindings/associated_remote.h"
#include "third_party/blink/public/mojom/frame/intrinsic_sizing_info.mojom-forward.h"
#include "third_party/blink/public/mojom/page/widget.mojom.h"

namespace content {

class FakeRenderWidgetHost : public blink::mojom::FrameWidgetHost,
                             public blink::mojom::WidgetHost {
 public:
  FakeRenderWidgetHost();
  ~FakeRenderWidgetHost() override;

  std::pair<mojo::PendingAssociatedRemote<blink::mojom::FrameWidgetHost>,
            mojo::PendingAssociatedReceiver<blink::mojom::FrameWidget>>
  BindNewFrameWidgetInterfaces();

  std::pair<mojo::PendingAssociatedRemote<blink::mojom::WidgetHost>,
            mojo::PendingAssociatedReceiver<blink::mojom::Widget>>
  BindNewWidgetInterfaces();

  // blink::mojom::FrameWidgetHost overrides.
  void AnimateDoubleTapZoomInMainFrame(const gfx::Point& tap_point,
                                       const gfx::Rect& rect_to_zoom) override;
  void ZoomToFindInPageRectInMainFrame(const gfx::Rect& rect_to_zoom) override;
  void SetHasTouchEventHandlers(bool has_handlers) override;
  void IntrinsicSizingInfoChanged(
      blink::mojom::IntrinsicSizingInfoPtr sizing_info) override;
  void AutoscrollStart(const gfx::PointF& position) override;
  void AutoscrollFling(const gfx::Vector2dF& position) override;
  void AutoscrollEnd() override;

  // blink::mojom::WidgetHost overrides.
  void SetCursor(const ui::Cursor& cursor) override;
  void SetToolTipText(const base::string16& tooltip_text,
                      base::i18n::TextDirection text_direction_hint) override;

  mojo::AssociatedReceiver<blink::mojom::WidgetHost>&
  widget_host_receiver_for_testing() {
    return widget_host_receiver_;
  }

 private:
  mojo::AssociatedReceiver<blink::mojom::FrameWidgetHost>
      frame_widget_host_receiver_{this};
  mojo::AssociatedRemote<blink::mojom::FrameWidget> frame_widget_remote_;
  mojo::AssociatedReceiver<blink::mojom::WidgetHost> widget_host_receiver_{
      this};
  mojo::AssociatedRemote<blink::mojom::Widget> widget_remote_;

  DISALLOW_COPY_AND_ASSIGN(FakeRenderWidgetHost);
};

}  // namespace content

#endif  // CONTENT_PUBLIC_TEST_FAKE_RENDER_WIDGET_HOST_H_
