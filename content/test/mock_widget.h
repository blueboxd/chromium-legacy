// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_TEST_MOCK_WIDGET_H_
#define CONTENT_TEST_MOCK_WIDGET_H_

#include <stddef.h>

#include <memory>
#include <utility>

#include "mojo/public/cpp/bindings/associated_receiver.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "third_party/blink/public/common/widget/visual_properties.h"
#include "third_party/blink/public/mojom/page/widget.mojom.h"

namespace content {

class MockWidget : public blink::mojom::Widget {
 public:
  MockWidget();

  ~MockWidget() override;

  mojo::PendingAssociatedRemote<blink::mojom::Widget> GetNewRemote();
  const std::vector<blink::VisualProperties>& ReceivedVisualProperties();
  void ClearVisualProperties();

  const std::vector<std::pair<gfx::Rect, gfx::Rect>>& ReceivedScreenRects();
  void ClearScreenRects();
  void SetTouchActionFromMain(cc::TouchAction touch_action);

  // blink::mojom::Widget overrides.
  void ForceRedraw(ForceRedrawCallback callback) override;

  void GetWidgetInputHandler(
      mojo::PendingReceiver<blink::mojom::WidgetInputHandler> request,
      mojo::PendingRemote<blink::mojom::WidgetInputHandlerHost> host) override;

  void UpdateVisualProperties(
      const blink::VisualProperties& visual_properties) override;

  void UpdateScreenRects(const gfx::Rect& widget_screen_rect,
                         const gfx::Rect& window_screen_rect,
                         UpdateScreenRectsCallback callback) override;

 private:
  std::vector<blink::VisualProperties> visual_properties_;
  std::vector<std::pair<gfx::Rect, gfx::Rect>> screen_rects_;
  std::vector<UpdateScreenRectsCallback> screen_rects_callbacks_;
  mojo::Remote<blink::mojom::WidgetInputHandlerHost> input_handler_host_;
  mojo::AssociatedReceiver<blink::mojom::Widget> blink_widget_{this};
};

}  // namespace content

#endif  // CONTENT_TEST_MOCK_WIDGET_H_
