// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/test/mock_widget.h"

namespace content {

MockWidget::MockWidget() = default;

MockWidget::~MockWidget() = default;

mojo::PendingAssociatedRemote<blink::mojom::Widget> MockWidget::GetNewRemote() {
  return blink_widget_.BindNewEndpointAndPassDedicatedRemoteForTesting();
}

const std::vector<blink::VisualProperties>&
MockWidget::ReceivedVisualProperties() {
  return visual_properties_;
}

void MockWidget::ClearVisualProperties() {
  visual_properties_.clear();
}

void MockWidget::ForceRedraw(ForceRedrawCallback callback) {}

void MockWidget::GetWidgetInputHandler(
    mojo::PendingReceiver<blink::mojom::WidgetInputHandler> request,
    mojo::PendingRemote<blink::mojom::WidgetInputHandlerHost> host) {}

void MockWidget::UpdateVisualProperties(
    const blink::VisualProperties& visual_properties) {
  visual_properties_.push_back(visual_properties);
}

}  // namespace content
