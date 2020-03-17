// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_PLATFORM_WIDGET_WIDGET_BASE_H_
#define THIRD_PARTY_BLINK_RENDERER_PLATFORM_WIDGET_WIDGET_BASE_H_

#include "base/time/time.h"
#include "mojo/public/cpp/bindings/associated_receiver.h"
#include "mojo/public/cpp/bindings/associated_remote.h"
#include "third_party/blink/public/mojom/page/widget.mojom-blink.h"
#include "third_party/blink/public/platform/cross_variant_mojo_util.h"
#include "third_party/blink/renderer/platform/platform_export.h"

namespace cc {
class AnimationHost;
class LayerTreeHost;
}  // namespace cc

namespace blink {
class WidgetBaseClient;

// This class is the foundational class for all widgets that blink creates.
// (WebPagePopupImpl, WebFrameWidgetBase) will contain an instance of this
// class. For simplicity purposes this class will be a member of those classes.
// It will eventually host compositing, input and emulation. See design doc:
// https://docs.google.com/document/d/10uBnSWBaitGsaROOYO155Wb83rjOPtrgrGTrQ_pcssY/edit?ts=5e3b26f7
class PLATFORM_EXPORT WidgetBase : public mojom::blink::Widget {
 public:
  WidgetBase(
      WidgetBaseClient* client,
      CrossVariantMojoAssociatedRemote<mojom::WidgetHostInterfaceBase>
          widget_host,
      CrossVariantMojoAssociatedReceiver<mojom::WidgetInterfaceBase> widget);
  ~WidgetBase() override;

  // Set the current compositor hosts.
  void SetCompositorHosts(cc::LayerTreeHost*, cc::AnimationHost*);

  // Set the compositor as visible. If |visible| is true, then the compositor
  // will request a new layer frame sink, begin producing frames from the
  // compositor scheduler, and in turn will update the document lifecycle.
  void SetCompositorVisible(bool visible);

  // Called to update the document lifecycle, advance the state of animations
  // and dispatch rAF.
  void BeginMainFrame(base::TimeTicks frame_time);

  // Update the visual state of the document, running the document lifecycle.
  void UpdateVisualState();

  // Called when a compositor frame will begin.
  void WillBeginCompositorFrame();

  cc::AnimationHost* AnimationHost() const;
  cc::LayerTreeHost* LayerTreeHost() const;

  // Returns if we should gather begin main frame metrics. If there is no
  // compositor thread this returns false.
  static bool ShouldRecordBeginMainFrameMetrics();

 private:
  // Not owned, they are owned by the RenderWidget.
  cc::LayerTreeHost* layer_tree_host_ = nullptr;
  cc::AnimationHost* animation_host_ = nullptr;
  WidgetBaseClient* client_;
  mojo::AssociatedRemote<mojom::blink::WidgetHost> widget_host_;
  mojo::AssociatedReceiver<mojom::blink::Widget> receiver_;
  bool first_update_visual_state_after_hidden_ = false;
  base::TimeTicks was_shown_time_ = base::TimeTicks::Now();
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_PLATFORM_WIDGET_WIDGET_BASE_H_
