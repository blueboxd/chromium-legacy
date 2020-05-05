// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_PLATFORM_WIDGET_WIDGET_BASE_H_
#define THIRD_PARTY_BLINK_RENDERER_PLATFORM_WIDGET_WIDGET_BASE_H_

#include "base/time/time.h"
#include "cc/paint/element_id.h"
#include "mojo/public/cpp/bindings/associated_receiver.h"
#include "mojo/public/cpp/bindings/associated_remote.h"
#include "third_party/blink/public/common/metrics/document_update_reason.h"
#include "third_party/blink/public/mojom/page/widget.mojom-blink.h"
#include "third_party/blink/public/platform/cross_variant_mojo_util.h"
#include "third_party/blink/public/web/web_widget.h"
#include "third_party/blink/renderer/platform/platform_export.h"
#include "third_party/blink/renderer/platform/weborigin/kurl.h"
#include "third_party/blink/renderer/platform/widget/compositing/layer_tree_view_delegate.h"

namespace cc {
class AnimationHost;
class LayerTreeHost;
class LayerTreeSettings;
class TaskGraphRunner;
class UkmRecorderFactory;
}  // namespace cc

namespace blink {
class LayerTreeView;
class WidgetBaseClient;

namespace scheduler {
class WebRenderWidgetSchedulingState;
}

// This class is the foundational class for all widgets that blink creates.
// (WebPagePopupImpl, WebFrameWidgetBase) will contain an instance of this
// class. For simplicity purposes this class will be a member of those classes.
// It will eventually host compositing, input and emulation. See design doc:
// https://docs.google.com/document/d/10uBnSWBaitGsaROOYO155Wb83rjOPtrgrGTrQ_pcssY/edit?ts=5e3b26f7
class PLATFORM_EXPORT WidgetBase : public mojom::blink::Widget,
                                   public LayerTreeViewDelegate {
 public:
  WidgetBase(
      WidgetBaseClient* client,
      CrossVariantMojoAssociatedRemote<mojom::WidgetHostInterfaceBase>
          widget_host,
      CrossVariantMojoAssociatedReceiver<mojom::WidgetInterfaceBase> widget);
  ~WidgetBase() override;

  // Initialize the compositor.
  void InitializeCompositing(
      cc::TaskGraphRunner* task_graph_runner,
      const cc::LayerTreeSettings& settings,
      std::unique_ptr<cc::UkmRecorderFactory> ukm_recorder_factory);

  // Shutdown the compositor.
  void Shutdown(scoped_refptr<base::SingleThreadTaskRunner> cleanup_runner,
                base::OnceCallback<void()> cleanup_task);

  // Set the compositor as visible. If |visible| is true, then the compositor
  // will request a new layer frame sink, begin producing frames from the
  // compositor scheduler, and in turn will update the document lifecycle.
  void SetCompositorVisible(bool visible);

  void AddPresentationCallback(
      uint32_t frame_token,
      base::OnceCallback<void(base::TimeTicks)> callback);

  // LayerTreeDelegate overrides:
  // Applies viewport related properties during a commit from the compositor
  // thread.
  void ApplyViewportChanges(const cc::ApplyViewportChangesArgs& args) override;
  void RecordManipulationTypeCounts(cc::ManipulationInfo info) override;
  void SendOverscrollEventFromImplSide(
      const gfx::Vector2dF& overscroll_delta,
      cc::ElementId scroll_latched_element_id) override;
  void SendScrollEndEventFromImplSide(
      cc::ElementId scroll_latched_element_id) override;
  void BeginMainFrame(base::TimeTicks frame_time) override;
  void OnDeferMainFrameUpdatesChanged(bool) override;
  void OnDeferCommitsChanged(bool) override;
  void DidBeginMainFrame() override;
  void RequestNewLayerTreeFrameSink(
      LayerTreeFrameSinkCallback callback) override;
  void DidCommitAndDrawCompositorFrame() override;
  void WillCommitCompositorFrame() override;
  void DidCommitCompositorFrame(base::TimeTicks commit_start_time) override;
  void DidCompletePageScaleAnimation() override;
  void RecordStartOfFrameMetrics() override;
  void RecordEndOfFrameMetrics(
      base::TimeTicks frame_begin_time,
      cc::ActiveFrameSequenceTrackers trackers) override;
  std::unique_ptr<cc::BeginMainFrameMetrics> GetBeginMainFrameMetrics()
      override;
  void BeginUpdateLayers() override;
  void EndUpdateLayers() override;
  void UpdateVisualState() override;
  void WillBeginMainFrame() override;

  cc::AnimationHost* AnimationHost() const;
  cc::LayerTreeHost* LayerTreeHost() const;
  scheduler::WebRenderWidgetSchedulingState* RendererWidgetSchedulingState()
      const;

  // Returns if we should gather begin main frame metrics. If there is no
  // compositor thread this returns false.
  static bool ShouldRecordBeginMainFrameMetrics();

 private:
  std::unique_ptr<LayerTreeView> layer_tree_view_;
  WidgetBaseClient* client_;
  mojo::AssociatedRemote<mojom::blink::WidgetHost> widget_host_;
  mojo::AssociatedReceiver<mojom::blink::Widget> receiver_;
  std::unique_ptr<scheduler::WebRenderWidgetSchedulingState>
      render_widget_scheduling_state_;
  bool first_update_visual_state_after_hidden_ = false;
  base::TimeTicks was_shown_time_ = base::TimeTicks::Now();
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_PLATFORM_WIDGET_WIDGET_BASE_H_
