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
#include "ui/gfx/geometry/point.h"
#include "ui/gfx/geometry/rect.h"

namespace content {

class FakeRenderWidgetHost : public blink::mojom::FrameWidgetHost {
 public:
  FakeRenderWidgetHost();
  ~FakeRenderWidgetHost() override;

  std::pair<mojo::PendingAssociatedRemote<blink::mojom::FrameWidgetHost>,
            mojo::PendingAssociatedReceiver<blink::mojom::FrameWidget>>
  BindNewFrameWidgetInterfaces();

  // blink::mojom::FrameWidgetHost overrides.
  void AnimateDoubleTapZoomInMainFrame(const gfx::Point& tap_point,
                                       const gfx::Rect& rect_to_zoom) override;
  void ZoomToFindInPageRectInMainFrame(const gfx::Rect& rect_to_zoom) override;
  void SetHasTouchEventHandlers(bool has_handlers) override;
  void IntrinsicSizingInfoChanged(
      blink::mojom::IntrinsicSizingInfoPtr sizing_info) override;

 private:
  mojo::AssociatedReceiver<blink::mojom::FrameWidgetHost>
      frame_widget_host_receiver_{this};
  mojo::AssociatedRemote<blink::mojom::FrameWidget> frame_widget_remote_;

  DISALLOW_COPY_AND_ASSIGN(FakeRenderWidgetHost);
};

}  // namespace content

#endif  // CONTENT_PUBLIC_TEST_FAKE_RENDER_WIDGET_HOST_H_
