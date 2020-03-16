// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/exported/web_external_widget_impl.h"

#include "cc/trees/layer_tree_host.h"

namespace blink {

std::unique_ptr<WebExternalWidget> WebExternalWidget::Create(
    WebExternalWidgetClient* client,
    const blink::WebURL& debug_url,
    CrossVariantMojoAssociatedRemote<mojom::blink::WidgetHostInterfaceBase>
        widget_host,
    CrossVariantMojoAssociatedReceiver<mojom::blink::WidgetInterfaceBase>
        widget) {
  return std::make_unique<WebExternalWidgetImpl>(
      client, debug_url, std::move(widget_host), std::move(widget));
}

WebExternalWidgetImpl::WebExternalWidgetImpl(
    WebExternalWidgetClient* client,
    const WebURL& debug_url,
    CrossVariantMojoAssociatedRemote<mojom::blink::WidgetHostInterfaceBase>
        widget_host,
    CrossVariantMojoAssociatedReceiver<mojom::blink::WidgetInterfaceBase>
        widget)
    : client_(client),
      debug_url_(debug_url),
      widget_base_(this, std::move(widget_host), std::move(widget)) {
  DCHECK(client_);
}

WebExternalWidgetImpl::~WebExternalWidgetImpl() = default;

void WebExternalWidgetImpl::SetCompositorHosts(
    cc::LayerTreeHost* layer_tree_host,
    cc::AnimationHost* animation_host) {
  widget_base_.SetCompositorHosts(layer_tree_host, animation_host);
}

WebHitTestResult WebExternalWidgetImpl::HitTestResultAt(const gfx::Point&) {
  NOTIMPLEMENTED();
  return {};
}

WebURL WebExternalWidgetImpl::GetURLForDebugTrace() {
  return debug_url_;
}

WebSize WebExternalWidgetImpl::Size() {
  return size_;
}

void WebExternalWidgetImpl::Resize(const WebSize& size) {
  if (size_ == size)
    return;
  size_ = size;
  client_->DidResize(gfx::Size(size));
}

WebInputEventResult WebExternalWidgetImpl::HandleInputEvent(
    const WebCoalescedInputEvent& coalesced_event) {
  return client_->HandleInputEvent(coalesced_event);
}

WebInputEventResult WebExternalWidgetImpl::DispatchBufferedTouchEvents() {
  return client_->DispatchBufferedTouchEvents();
}

void WebExternalWidgetImpl::SetRootLayer(scoped_refptr<cc::Layer> layer) {
  widget_base_.LayerTreeHost()->SetNonBlinkManagedRootLayer(layer);
}

}  // namespace blink
