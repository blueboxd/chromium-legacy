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
#include "third_party/blink/renderer/platform/text/text_direction.h"
#include "third_party/blink/renderer/platform/weborigin/kurl.h"
#include "third_party/blink/renderer/platform/widget/compositing/layer_tree_view_delegate.h"
#include "third_party/blink/renderer/platform/widget/input/widget_base_input_handler.h"
#include "ui/base/ime/text_input_mode.h"
#include "ui/base/ime/text_input_type.h"

namespace cc {
class AnimationHost;
class LayerTreeHost;
class LayerTreeSettings;
class TaskGraphRunner;
class UkmRecorderFactory;
}  // namespace cc

namespace ui {
class Cursor;
}

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

  // mojom::blink::Widget overrides:
  void ForceRedraw(mojom::blink::Widget::ForceRedrawCallback callback) override;
  void GetWidgetInputHandler(
      mojo::PendingReceiver<mojom::blink::WidgetInputHandler> request,
      mojo::PendingRemote<mojom::blink::WidgetInputHandlerHost> host) override;

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
  void DidObserveFirstScrollDelay(
      base::TimeDelta first_scroll_delay,
      base::TimeTicks first_scroll_timestamp) override;
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
  void SubmitThroughputData(ukm::SourceId source_id,
                            int aggregated_percent,
                            int impl_percent,
                            base::Optional<int> main_percent) override;

  cc::AnimationHost* AnimationHost() const;
  cc::LayerTreeHost* LayerTreeHost() const;
  scheduler::WebRenderWidgetSchedulingState* RendererWidgetSchedulingState()
      const;

  mojom::blink::WidgetHost* GetWidgetHostRemote() { return widget_host_.get(); }

  // Returns if we should gather begin main frame metrics. If there is no
  // compositor thread this returns false.
  static bool ShouldRecordBeginMainFrameMetrics();

  // Set the current cursor relay to browser if necessary.
  void SetCursor(const ui::Cursor& cursor);

  // Dispatch the virtual keyboard and update text input state.
  void ShowVirtualKeyboardOnElementFocus();

  // Process the touch action, return true if the action should be
  // sent to the browser.
  bool ProcessTouchAction(cc::TouchAction touch_action);

  WidgetBaseInputHandler& input_handler() { return input_handler_; }

  WidgetBaseClient* client() { return client_; }

  void SetToolTipText(const String& tooltip_text, TextDirection dir);

  void ShowVirtualKeyboard();
  void UpdateSelectionBounds();
  void UpdateTextInputState();
  void ClearTextInputState();
  void ForceTextInputStateUpdate();
  void RequestCompositionUpdates(bool immediate_request, bool monitor_updates);
  void UpdateCompositionInfo(bool immediate_request);
  void SetFocus(bool enable);
  bool has_focus() const { return has_focus_; }

 private:
  bool CanComposeInline();
  void UpdateTextInputStateInternal(bool show_virtual_keyboard,
                                    bool immediate_request);
  void GetCompositionRange(gfx::Range* range);
  void GetCompositionCharacterBounds(Vector<gfx::Rect>* bounds);
  ui::TextInputType GetTextInputType();
  bool ShouldUpdateCompositionInfo(const gfx::Range& range,
                                   const Vector<gfx::Rect>& bounds);

  std::unique_ptr<LayerTreeView> layer_tree_view_;
  WidgetBaseClient* client_;
  mojo::AssociatedRemote<mojom::blink::WidgetHost> widget_host_;
  mojo::AssociatedReceiver<mojom::blink::Widget> receiver_;
  std::unique_ptr<scheduler::WebRenderWidgetSchedulingState>
      render_widget_scheduling_state_;
  bool first_update_visual_state_after_hidden_ = false;
  base::TimeTicks was_shown_time_ = base::TimeTicks::Now();
  bool has_focus_ = false;
  WidgetBaseInputHandler input_handler_{this};

  // Stores the current selection bounds.
  gfx::Rect selection_focus_rect_;
  gfx::Rect selection_anchor_rect_;

  // Stores the current composition character bounds.
  Vector<gfx::Rect> composition_character_bounds_;

  // Stores the current composition range.
  gfx::Range composition_range_ = gfx::Range::InvalidRange();

  // True if the IME requests updated composition info.
  bool monitor_composition_info_ = false;
  // Stores information about the current text input.
  blink::WebTextInputInfo text_input_info_;

  // Stores the current text input type of |webwidget_|.
  ui::TextInputType text_input_type_ = ui::TEXT_INPUT_TYPE_NONE;

  // Stores the current text input mode of |webwidget_|.
  ui::TextInputMode text_input_mode_ = ui::TEXT_INPUT_MODE_DEFAULT;

  // Stores the current virtualkeyboardpolicy of |webwidget_|.
  ui::mojom::VirtualKeyboardPolicy vk_policy_ =
      ui::mojom::VirtualKeyboardPolicy::AUTO;

  // Stores the current text input flags of |webwidget_|.
  int text_input_flags_ = 0;

  // Indicates whether currently focused input field has next/previous focusable
  // form input field.
  int next_previous_flags_;

  // Stores the current type of composition text rendering of |webwidget_|.
  bool can_compose_inline_ = true;

  // Stores whether the IME should always be hidden for |webwidget_|.
  bool always_hide_ime_ = false;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_PLATFORM_WIDGET_WIDGET_BASE_H_
