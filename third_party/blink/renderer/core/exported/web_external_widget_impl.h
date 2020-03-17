// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_EXPORTED_WEB_EXTERNAL_WIDGET_IMPL_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_EXPORTED_WEB_EXTERNAL_WIDGET_IMPL_H_

#include "third_party/blink/public/web/web_external_widget.h"

#include "third_party/blink/public/mojom/page/widget.mojom-blink.h"
#include "third_party/blink/public/platform/cross_variant_mojo_util.h"
#include "third_party/blink/public/platform/web_url.h"
#include "third_party/blink/renderer/platform/widget/widget_base_client.h"

namespace blink {
class WidgetBase;

class WebExternalWidgetImpl : public WebExternalWidget,
                              public WidgetBaseClient {
 public:
  WebExternalWidgetImpl(
      WebExternalWidgetClient* client,
      const WebURL& debug_url,
      CrossVariantMojoAssociatedRemote<mojom::blink::WidgetHostInterfaceBase>
          widget_host,
      CrossVariantMojoAssociatedReceiver<mojom::blink::WidgetInterfaceBase>
          widget);
  ~WebExternalWidgetImpl() override;

  // WebWidget overrides:
  void SetCompositorHosts(cc::LayerTreeHost*, cc::AnimationHost*) override;
  void SetCompositorVisible(bool visible) override;
  void UpdateVisualState() override;
  void WillBeginCompositorFrame() override;
  WebHitTestResult HitTestResultAt(const gfx::Point&) override;
  WebURL GetURLForDebugTrace() override;
  WebSize Size() override;
  void Resize(const WebSize& size) override;
  WebInputEventResult HandleInputEvent(
      const WebCoalescedInputEvent& coalesced_event) override;
  WebInputEventResult DispatchBufferedTouchEvents() override;

  // WebExternalWidget overrides:
  void SetRootLayer(scoped_refptr<cc::Layer>) override;

  // WidgetBaseClient overrides:
  void DispatchRafAlignedInput(base::TimeTicks frame_time) override {}
  void BeginMainFrame(base::TimeTicks last_frame_time) override {}
  void RecordTimeToFirstActivePaint(base::TimeDelta duration) override;
  void UpdateLifecycle(WebLifecycleUpdate requested_update,
                       DocumentUpdateReason reason) override {}

 private:
  WebExternalWidgetClient* const client_;
  const WebURL debug_url_;
  WebSize size_;
  std::unique_ptr<WidgetBase> widget_base_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_EXPORTED_WEB_EXTERNAL_WIDGET_IMPL_H_
