// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_RENDERER_RENDER_WIDGET_H_
#define CONTENT_RENDERER_RENDER_WIDGET_H_

#include <stddef.h>
#include <stdint.h>

#include <map>
#include <memory>
#include <queue>
#include <string>
#include <vector>

#include "base/callback.h"
#include "base/cancelable_callback.h"
#include "base/compiler_specific.h"
#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/memory/weak_ptr.h"
#include "base/observer_list.h"
#include "base/single_thread_task_runner.h"
#include "base/time/time.h"
#include "build/build_config.h"
#include "cc/input/overscroll_behavior.h"
#include "cc/input/touch_action.h"
#include "cc/trees/layer_tree_settings.h"
#include "cc/trees/managed_memory_policy.h"
#include "components/viz/common/surfaces/local_surface_id.h"
#include "content/common/buildflags.h"
#include "content/common/content_export.h"
#include "content/common/cursors/webcursor.h"
#include "content/common/drag_event_source_info.h"
#include "content/common/edit_command.h"
#include "content/common/tab_switch_time_recorder.h"
#include "content/common/widget.mojom.h"
#include "content/public/common/drop_data.h"
#include "content/public/common/screen_info.h"
#include "content/renderer/compositor/layer_tree_view_delegate.h"
#include "content/renderer/input/main_thread_event_queue.h"
#include "content/renderer/input/render_widget_input_handler.h"
#include "content/renderer/input/render_widget_input_handler_delegate.h"
#include "content/renderer/mouse_lock_dispatcher.h"
#include "content/renderer/render_widget_delegate.h"
#include "content/renderer/render_widget_mouse_lock_dispatcher.h"
#include "content/renderer/render_widget_screen_metrics_emulator_delegate.h"
#include "ipc/ipc_listener.h"
#include "ipc/ipc_message.h"
#include "ipc/ipc_sender.h"
#include "mojo/public/cpp/bindings/binding.h"
#include "ppapi/buildflags/buildflags.h"
#include "services/network/public/mojom/referrer_policy.mojom.h"
#include "third_party/blink/public/common/frame/occlusion_state.h"
#include "third_party/blink/public/common/manifest/web_display_mode.h"
#include "third_party/blink/public/platform/web_input_event.h"
#include "third_party/blink/public/platform/web_rect.h"
#include "third_party/blink/public/platform/web_text_input_info.h"
#include "third_party/blink/public/web/web_ime_text_span.h"
#include "third_party/blink/public/web/web_page_popup.h"
#include "third_party/blink/public/web/web_text_direction.h"
#include "third_party/blink/public/web/web_widget.h"
#include "third_party/blink/public/web/web_widget_client.h"
#include "ui/base/ime/text_input_mode.h"
#include "ui/base/ime/text_input_type.h"
#include "ui/base/ui_base_types.h"
#include "ui/events/types/scroll_types.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/geometry/vector2d_f.h"
#include "ui/gfx/native_widget_types.h"
#include "ui/gfx/range/range.h"
#include "ui/surface/transport_dib.h"

namespace IPC {
class SyncMessageFilter;
}

namespace blink {
namespace scheduler {
class WebRenderWidgetSchedulingState;
}
struct WebDeviceEmulationParams;
class WebDragData;
class WebFrameWidget;
class WebInputMethodController;
class WebLocalFrame;
class WebMouseEvent;
class WebPagePopup;
}  // namespace blink

namespace cc {
struct ApplyViewportChangesArgs;
class SwapPromise;
}

namespace gfx {
class ColorSpace;
struct PresentationFeedback;
class Range;
}

namespace ui {
struct DidOverscrollParams;
}

namespace content {
class BrowserPlugin;
class CompositorDependencies;
class ExternalPopupMenu;
class FrameSwapMessageQueue;
class ImeEventGuard;
class LayerTreeView;
class MainThreadEventQueue;
class PepperPluginInstanceImpl;
class RenderFrameImpl;
class RenderFrameProxy;
class RenderViewImpl;
class RenderWidgetDelegate;
class RenderWidgetScreenMetricsEmulator;
class TextInputClientObserver;
class WidgetInputHandlerManager;
struct ContextMenuParams;
struct VisualProperties;

// RenderWidget provides a communication bridge between a WebWidget and
// a RenderWidgetHost, the latter of which lives in a different process.
//
// RenderWidget is used to implement:
// - RenderViewImpl (deprecated)
// - Fullscreen mode (RenderWidgetFullScreen)
// - Popup "menus" (like the color chooser and date picker)
// - Widgets for frames (the main frame, and subframes due to out-of-process
//   iframe support)
//
// Because the main frame RenderWidget is used to implement RenderViewImpl
// (deprecated) it has a shared lifetime. But the RenderViewImpl may have
// a proxy main frame which does not use a RenderWidget. Thus a RenderWidget
// can be frozen, during the time in which we wish we could delete it but we
// can't, and it should be (relatively.. it's a work in progress) unused during
// that time. This does not apply to subframes, whose lifetimes are not tied to
// the RenderViewImpl.
class CONTENT_EXPORT RenderWidget
    : public IPC::Listener,
      public IPC::Sender,
      public blink::WebPagePopupClient,  // Is-a WebWidgetClient also.
      public mojom::Widget,
      public LayerTreeViewDelegate,
      public RenderWidgetInputHandlerDelegate,
      public RenderWidgetScreenMetricsEmulatorDelegate,
      public base::RefCounted<RenderWidget>,
      public MainThreadEventQueueClient {
 public:
  using ShowCallback =
      base::OnceCallback<void(RenderWidget* widget_to_show,
                              blink::WebNavigationPolicy policy,
                              const gfx::Rect& initial_rect)>;

  // Time-To-First-Active-Paint(TTFAP) type
  enum {
    TTFAP_AFTER_PURGED,
    TTFAP_5MIN_AFTER_BACKGROUNDED,
  };

  // Convenience type for creation method taken by InstallCreateForFrameHook().
  // The method signature matches the RenderWidget constructor.
  using CreateRenderWidgetFunction =
      scoped_refptr<RenderWidget> (*)(int32_t,
                                      CompositorDependencies*,
                                      const ScreenInfo&,
                                      blink::WebDisplayMode display_mode,
                                      bool,
                                      bool,
                                      bool,
                                      mojom::WidgetRequest widget_request);
  // Overrides the implementation of CreateForFrame() function below. Used by
  // web tests to return a partial fake of RenderWidget.
  static void InstallCreateForFrameHook(
      CreateRenderWidgetFunction create_widget);

  // Creates a RenderWidget that is meant to be associated with a RenderFrame.
  // Testing infrastructure, such as test_runner, can override this function
  // by calling InstallCreateForFrameHook().
  static scoped_refptr<RenderWidget> CreateForFrame(
      int32_t widget_routing_id,
      CompositorDependencies* compositor_deps,
      const ScreenInfo& screen_info,
      blink::WebDisplayMode display_mode,
      bool is_frozen,
      bool hidden,
      bool never_visible,
      mojom::WidgetRequest widget_request);

  // Creates a RenderWidget for a popup. This is separate from CreateForFrame()
  // because popups do not not need to be faked out.
  static scoped_refptr<RenderWidget> CreateForPopup(
      int32_t widget_routing_id,
      CompositorDependencies* compositor_deps,
      const ScreenInfo& screen_info,
      blink::WebDisplayMode display_mode,
      bool is_frozen,
      bool hidden,
      bool never_visible,
      mojom::WidgetRequest widget_request);

  // Initialize a new RenderWidget for a popup. The |show_callback| is called
  // when RenderWidget::Show() happens. This method increments the reference
  // count on the RenderWidget, making it self-referencing, which is then
  // release when a WidgetMsg_Close IPC is received.
  void InitForPopup(ShowCallback show_callback,
                    blink::WebPagePopup* web_page_popup);

  // Initialize a new RenderWidget that will be attached to a RenderFrame (via
  // the WebFrameWidget), for a frame that is a local root, but not the main
  // frame. This method increments the reference count on the RenderWidget,
  // making it self-referencing, which can be released by calling Close().
  void InitForChildLocalRoot(blink::WebFrameWidget* web_frame_widget);

  // Sets a delegate to handle certain RenderWidget operations that need an
  // escape to the RenderView. Also take ownership until RenderWidget lifetime
  // has been reassociated with the RenderFrame. This is only set on Widgets
  // that are associated with the RenderView.
  // TODO(ajwong): Do not have RenderWidget own the delegate.
  void set_delegate(std::unique_ptr<RenderWidgetDelegate> delegate) {
    DCHECK(!delegate_);
    delegate_ = std::move(delegate);
  }

  RenderWidgetDelegate* delegate() const { return delegate_.get(); }

  // Returns the RenderWidget for the given routing ID.
  static RenderWidget* FromRoutingID(int32_t routing_id);

  // Closes a RenderWidget that was created by |CreateForFrame|.
  void CloseForFrame();

  int32_t routing_id() const { return routing_id_; }

  CompositorDependencies* compositor_deps() const { return compositor_deps_; }

  // This can return nullptr while the RenderWidget is closing.
  blink::WebWidget* GetWebWidget() const;

  // Returns the current instance of WebInputMethodController which is to be
  // used for IME related tasks. This instance corresponds to the one from
  // focused frame and can be nullptr.
  blink::WebInputMethodController* GetInputMethodController() const;

  const gfx::Size& size() const { return size_; }
  bool is_fullscreen_granted() const { return is_fullscreen_granted_; }
  blink::WebDisplayMode display_mode() const { return display_mode_; }
  bool is_hidden() const { return is_hidden_; }
  // Temporary for debugging purposes...
  bool closing() const { return closing_; }
  bool has_host_context_menu_location() const {
    return has_host_context_menu_location_;
  }
  gfx::Point host_context_menu_location() const {
    return host_context_menu_location_;
  }
  const gfx::Size& visible_viewport_size() const {
    return visible_viewport_size_;
  }

  ScreenInfo screen_info() const { return screen_info_; }
  void set_screen_info(const ScreenInfo& info) { screen_info_ = info; }

  // Sets whether this RenderWidget should be moved into or out of a frozen
  // state. This state is used for the RenderWidget attached to a RenderViewImpl
  // for its main frame, when there is no local main frame present.
  // In this case, the RenderWidget can't be deleted currently but should
  // otherwise act as if it is dead. Only whitelisted new IPC messages will be
  // sent, and it does no compositing. The process is free to exit when there
  // are no other unfrozen (thawed) RenderWidgets.
  void SetIsFrozen(bool is_frozen);
  bool is_frozen() const { return is_frozen_; }

  // When a RenderWidget is created, even if frozen, if we expect to unfreeze
  // and use the RenderWidget imminently, then we want to pre-emptively start
  // the process of getting the resources needed for the compositor. This helps
  // to parallelize the critical path to first pixels with the loading process.
  // This should only be called when the RenderWidget is frozen, otherwise it
  // would be redundant at best. Non-frozen RenderWidgets will start to warmup
  // immediately on their own.
  void WarmupCompositor();
  // If after calling WarmupCompositor() we can determine that the RenderWidget
  // does not expect to be used shortly after all, call this to cancel the
  // warmup process and release any unused resources that had been created by
  // it.
  void AbortWarmupCompositor();

  // This is true once a Close IPC has been received. The actual action of
  // closing must be done on another stack frame, in case the IPC receipt
  // is in a nested message loop and will unwind back up to javascript (from
  // plugins). So this will be true between those two things, to avoid work
  // when the RenderWidget will be closed.
  // Additionally, as an optimization, this is true after we know the renderer
  // has asked the browser to close this RenderWidget.
  //
  // TODO(crbug.com/545684): Once RenderViewImpl and RenderWidget are split,
  // attempt to combine two states so the shutdown logic is cleaner.
  bool is_closing() const { return host_will_close_this_ || closing_; }

  // Manage edit commands to be used for the next keyboard event.
  const EditCommands& edit_commands() const { return edit_commands_; }
  void SetEditCommandForNextKeyEvent(const std::string& name,
                                     const std::string& value);
  void ClearEditCommands();

  // Functions to track out-of-process frames for special notifications.
  void RegisterRenderFrameProxy(RenderFrameProxy* proxy);
  void UnregisterRenderFrameProxy(RenderFrameProxy* proxy);

  // Functions to track all RenderFrame objects associated with this
  // RenderWidget.
  void RegisterRenderFrame(RenderFrameImpl* frame);
  void UnregisterRenderFrame(RenderFrameImpl* frame);

  // BrowserPlugins embedded by this RenderWidget register themselves here.
  // These plugins need to be notified about changes to ScreenInfo.
  void RegisterBrowserPlugin(BrowserPlugin* browser_plugin);
  void UnregisterBrowserPlugin(BrowserPlugin* browser_plugin);

  // IPC::Listener
  bool OnMessageReceived(const IPC::Message& msg) override;

  // IPC::Sender
  bool Send(IPC::Message* msg) override;

  // LayerTreeViewDelegate
  void ApplyViewportChanges(const cc::ApplyViewportChangesArgs& args) override;
  void RecordManipulationTypeCounts(cc::ManipulationInfo info) override;
  void SendOverscrollEventFromImplSide(
      const gfx::Vector2dF& overscroll_delta,
      cc::ElementId scroll_latched_element_id) override;
  void SendScrollEndEventFromImplSide(
      cc::ElementId scroll_latched_element_id) override;
  void BeginMainFrame(base::TimeTicks frame_time) override;
  void DidBeginMainFrame() override;
  void RequestNewLayerTreeFrameSink(
      LayerTreeFrameSinkCallback callback) override;
  void DidCommitAndDrawCompositorFrame() override;
  void WillCommitCompositorFrame() override;
  void DidCommitCompositorFrame() override;
  void DidCompletePageScaleAnimation() override;
  void RecordStartOfFrameMetrics() override;
  void RecordEndOfFrameMetrics(base::TimeTicks frame_begin_time) override;
  void BeginUpdateLayers() override;
  void EndUpdateLayers() override;
  void UpdateVisualState() override;
  void WillBeginCompositorFrame() override;

  // RenderWidgetInputHandlerDelegate
  void FocusChangeComplete() override;
  void ObserveGestureEventAndResult(
      const blink::WebGestureEvent& gesture_event,
      const gfx::Vector2dF& unused_delta,
      const cc::OverscrollBehavior& overscroll_behavior,
      bool event_processed) override;

  void OnDidHandleKeyEvent() override;
  void OnDidOverscroll(const ui::DidOverscrollParams& params) override;
  void SetInputHandler(RenderWidgetInputHandler* input_handler) override;
  void ShowVirtualKeyboard() override;
  void UpdateTextInputState() override;
  void ClearTextInputState() override;
  bool WillHandleGestureEvent(const blink::WebGestureEvent& event) override;
  bool WillHandleMouseEvent(const blink::WebMouseEvent& event) override;

  // RenderWidgetScreenMetricsEmulatorDelegate
  void SynchronizeVisualProperties(
      const VisualProperties& resize_params) override;
  void SetScreenMetricsEmulationParameters(
      bool enabled,
      const blink::WebDeviceEmulationParams& params) override;
  void SetScreenRects(const gfx::Rect& widget_screen_rect,
                      const gfx::Rect& window_screen_rect) override;

  // blink::WebWidgetClient
  void SetLayerTreeMutator(std::unique_ptr<cc::LayerTreeMutator>) override;
  void SetPaintWorkletLayerPainterClient(
      std::unique_ptr<cc::PaintWorkletLayerPainter>) override;
  void SetRootLayer(scoped_refptr<cc::Layer> layer) override;
  void ScheduleAnimation() override;
  void SetShowFPSCounter(bool show) override;
  void SetShowLayoutShiftRegions(bool) override;
  void SetShowPaintRects(bool) override;
  void SetShowDebugBorders(bool) override;
  void SetShowScrollBottleneckRects(bool) override;
  void SetShowHitTestBorders(bool) override;
  void SetBackgroundColor(SkColor color) override;
  void IntrinsicSizingInfoChanged(
      const blink::WebIntrinsicSizingInfo&) override;
  void DidMeaningfulLayout(blink::WebMeaningfulLayout layout_type) override;
  void DidChangeCursor(const blink::WebCursorInfo&) override;
  void AutoscrollStart(const blink::WebFloatPoint& point) override;
  void AutoscrollFling(const blink::WebFloatSize& velocity) override;
  void AutoscrollEnd() override;
  void ClosePopupWidgetSoon() override;
  void Show(blink::WebNavigationPolicy) override;
  blink::WebRect WindowRect() override;
  blink::WebRect ViewRect() override;
  void SetToolTipText(const blink::WebString& text,
                      blink::WebTextDirection hint) override;
  void SetWindowRect(const blink::WebRect&) override;
  void DidHandleGestureEvent(const blink::WebGestureEvent& event,
                             bool event_cancelled) override;
  void DidOverscroll(const blink::WebFloatSize& overscroll_delta,
                     const blink::WebFloatSize& accumulated_overscroll,
                     const blink::WebFloatPoint& position,
                     const blink::WebFloatSize& velocity) override;
  void InjectGestureScrollEvent(
      blink::WebGestureDevice device,
      const blink::WebFloatSize& delta,
      ui::input_types::ScrollGranularity granularity,
      cc::ElementId scrollable_area_element_id,
      blink::WebInputEvent::Type injected_type) override;
  void SetOverscrollBehavior(const cc::OverscrollBehavior&) override;
  void ShowVirtualKeyboardOnElementFocus() override;
  void ConvertViewportToWindow(blink::WebRect* rect) override;
  void ConvertWindowToViewport(blink::WebFloatRect* rect) override;
  bool RequestPointerLock() override;
  void RequestPointerUnlock() override;
  bool IsPointerLocked() override;
  void StartDragging(network::mojom::ReferrerPolicy policy,
                     const blink::WebDragData& data,
                     blink::WebDragOperationsMask mask,
                     const SkBitmap& drag_image,
                     const gfx::Point& image_offset) override;
  void SetTouchAction(cc::TouchAction touch_action) override;
  void RequestUnbufferedInputEvents() override;
  void HasPointerRawUpdateEventHandlers(bool has_handlers) override;
  void HasTouchEventHandlers(bool has_handlers) override;
  void SetNeedsLowLatencyInput(bool) override;
  void SetNeedsUnbufferedInputForDebugger(bool) override;
  void AnimateDoubleTapZoomInMainFrame(const blink::WebPoint& point,
                                       const blink::WebRect& bounds) override;
  void ZoomToFindInPageRectInMainFrame(
      const blink::WebRect& rect_to_zoom) override;
  void RegisterViewportLayers(
      const cc::ViewportLayers& viewport_layers) override;
  void RegisterSelection(const cc::LayerSelection& selection) override;
  void FallbackCursorModeLockCursor(bool left,
                                    bool right,
                                    bool up,
                                    bool down) override;
  void FallbackCursorModeSetCursorVisibility(bool visible) override;
  void SetAllowGpuRasterization(bool allow_gpu_raster) override;
  void SetPageScaleStateAndLimits(float page_scale_factor,
                                  bool is_pinch_gesture_active,
                                  float minimum,
                                  float maximum) override;
  void StartPageScaleAnimation(const gfx::Vector2d& destination,
                               bool use_anchor,
                               float new_page_scale,
                               double duration_sec) override;
  void RequestDecode(const cc::PaintImage& image,
                     base::OnceCallback<void(bool)> callback) override;
  void NotifySwapTime(ReportTimeCallback callback) override;

  // Registers a SwapPromise to report presentation time and possibly swap time.
  // If |swap_time_callback| is not a null callback, it would be called once
  // swap happens. |presentation_time_callback| will be called some time after
  // pixels are presented on screen. Swap time is needed only in tests and
  // production code uses |NotifySwapTime()| above which calls this one passing
  // a null callback as |swap_time_callback|.
  void NotifySwapAndPresentationTime(
      ReportTimeCallback swap_time_callback,
      ReportTimeCallback presentation_time_callback);

  // Override point to obtain that the current input method state and caret
  // position.
  ui::TextInputType GetTextInputType();

  // Sends a request to the browser to close this RenderWidget.
  void CloseWidgetSoon();

  static cc::LayerTreeSettings GenerateLayerTreeSettings(
      CompositorDependencies* compositor_deps,
      bool is_for_subframe,
      const gfx::Size& initial_screen_size,
      float initial_device_scale_factor);
  static cc::ManagedMemoryPolicy GetGpuMemoryPolicy(
      const cc::ManagedMemoryPolicy& policy,
      const gfx::Size& initial_screen_size,
      float initial_device_scale_factor);

  LayerTreeView* layer_tree_view() const { return layer_tree_view_.get(); }
  WidgetInputHandlerManager* widget_input_handler_manager() {
    return widget_input_handler_manager_.get();
  }
  const RenderWidgetInputHandler& input_handler() const {
    return *input_handler_;
  }

  void SetHandlingInputEvent(bool handling_input_event);

  // Queues the IPC |message| to be sent to the browser, delaying sending until
  // the next compositor frame submission. At that time they will be sent before
  // any message from the compositor as part of submitting its frame. This is
  // used for messages that need synchronization with the compositor, but in
  // general you should use Send().
  //
  // This mechanism is not a drop-in replacement for IPC: messages sent this way
  // will not be automatically available to BrowserMessageFilter, for example.
  // FIFO ordering is preserved between messages enqueued.
  //
  // |msg| message to send, ownership of |msg| is transferred.
  void QueueMessage(IPC::Message* msg);

  // Handle start and finish of IME event guard.
  void OnImeEventGuardStart(ImeEventGuard* guard);
  void OnImeEventGuardFinish(ImeEventGuard* guard);

  void ApplyEmulatedScreenMetricsForPopupWidget(RenderWidget* origin_widget);

  gfx::Rect AdjustValidationMessageAnchor(const gfx::Rect& anchor);

  // Checks if the selection bounds have been changed. If they are changed,
  // the new value will be sent to the browser process.
  void UpdateSelectionBounds();

  void GetSelectionBounds(gfx::Rect* start, gfx::Rect* end);

  void OnShowHostContextMenu(ContextMenuParams* params);

  // Checks if the composition range or composition character bounds have been
  // changed. If they are changed, the new value will be sent to the browser
  // process. This method does nothing when the browser process is not able to
  // handle composition range and composition character bounds.
  // If immediate_request is true, render sends the latest composition info to
  // the browser even if the composition info is not changed.
  void UpdateCompositionInfo(bool immediate_request);

  // Override point to obtain that the current composition character bounds.
  // In the case of surrogate pairs, the character is treated as two characters:
  // the bounds for first character is actual one, and the bounds for second
  // character is zero width rectangle.
  void GetCompositionCharacterBounds(std::vector<gfx::Rect>* character_bounds);

  // Called when the Widget has changed size as a result of an auto-resize.
  void DidAutoResize(const gfx::Size& new_size);

  void DidPresentForceDrawFrame(int snapshot_id,
                                const gfx::PresentationFeedback& feedback);

  // Indicates whether this widget has focus.
  bool has_focus() const { return has_focus_; }

  MouseLockDispatcher* mouse_lock_dispatcher() const {
    return mouse_lock_dispatcher_.get();
  }

  // Returns the ScreenInfo exposed to Blink. In device emulation, this
  // may not match the compositor ScreenInfo.
  const ScreenInfo& GetWebScreenInfo() const;

  // When emulated, this returns the original (non-emulated) ScreenInfo.
  const ScreenInfo& GetOriginalScreenInfo() const;

  // Helper to convert |point| using ConvertWindowToViewport().
  gfx::PointF ConvertWindowPointToViewport(const gfx::PointF& point);
  gfx::Point ConvertWindowPointToViewport(const gfx::Point& point);
  void DidNavigate();

  bool auto_resize_mode() const { return auto_resize_mode_; }

  const gfx::Size& min_size_for_auto_resize() const {
    return min_size_for_auto_resize_;
  }

  const gfx::Size& max_size_for_auto_resize() const {
    return max_size_for_auto_resize_;
  }

  uint32_t capture_sequence_number() const {
    return last_capture_sequence_number_;
  }

  // Returns true if a page scale animation is active.
  bool HasPendingPageScaleAnimation() const;

  // MainThreadEventQueueClient overrides.
  bool HandleInputEvent(const blink::WebCoalescedInputEvent& input_event,
                        const ui::LatencyInfo& latency_info,
                        HandledEventCallback callback) override;
  void SetNeedsMainFrame() override;

  viz::FrameSinkId GetFrameSinkIdAtPoint(const gfx::PointF& point,
                                         gfx::PointF* local_point);

  // Widget mojom overrides.
  void SetupWidgetInputHandler(mojom::WidgetInputHandlerRequest request,
                               mojom::WidgetInputHandlerHostPtr host) override;

  scoped_refptr<MainThreadEventQueue> GetInputEventQueue();

  void OnSetActive(bool active);
  void OnSetFocus(bool enable);
  void OnMouseCaptureLost();
  void OnCursorVisibilityChange(bool is_visible);
  void OnFallbackCursorModeToggled(bool is_on);
  void OnSetEditCommandsForNextKeyEvent(const EditCommands& edit_commands);
  void OnImeSetComposition(
      const base::string16& text,
      const std::vector<blink::WebImeTextSpan>& ime_text_spans,
      const gfx::Range& replacement_range,
      int selection_start,
      int selection_end);
  void OnImeCommitText(const base::string16& text,
                       const std::vector<blink::WebImeTextSpan>& ime_text_spans,
                       const gfx::Range& replacement_range,
                       int relative_cursor_pos);
  void OnImeFinishComposingText(bool keep_selection);

  // This does the actual focus change, but is called in more situations than
  // just as an IPC message.
  void SetFocus(bool enable);

  // Called by the browser process to update text input state.
  void OnRequestTextInputStateUpdate();

  // Called by the browser process to update the cursor and composition
  // information by sending WidgetInputHandlerHost::ImeCompositionRangeChanged.
  // If |immediate_request| is true, an IPC is sent back with current state.
  // When |monitor_update| is true, then RenderWidget will send the updates
  // in each compositor frame when there are changes. Outside of compositor
  // frame updates, a change in text selection might also lead to an update for
  // composition info (when in monitor mode).
  void OnRequestCompositionUpdates(bool immediate_request,
                                   bool monitor_updates);
  void SetWidgetBinding(mojom::WidgetRequest request);

  void SetMouseCapture(bool capture);

  bool IsSurfaceSynchronizationEnabled() const;

  void UseSynchronousResizeModeForTesting(bool enable);
  void SetDeviceScaleFactorForTesting(float factor);
  void SetDeviceColorSpaceForTesting(const gfx::ColorSpace& color_space);
  void SetWindowRectSynchronouslyForTesting(const gfx::Rect& new_window_rect);
  void EnableAutoResizeForTesting(const gfx::Size& min_size,
                                  const gfx::Size& max_size);
  void DisableAutoResizeForTesting(const gfx::Size& new_size);

  // Update the WebView's device scale factor.
  // TODO(ajwong): This should be moved into RenderView.
  void UpdateWebViewWithDeviceScaleFactor();

  // Forces a redraw and invokes the callback once the frame's been displayed
  // to the user.
  using PresentationTimeCallback =
      base::OnceCallback<void(const gfx::PresentationFeedback&)>;
  virtual void RequestPresentation(PresentationTimeCallback callback);

  // RenderWidget IPC message handler that can be overridden by subclasses.
  virtual void OnSynchronizeVisualProperties(const VisualProperties& params);

  bool in_synchronous_composite_for_testing() const {
    return in_synchronous_composite_for_testing_;
  }
  void set_in_synchronous_composite_for_testing(bool in) {
    in_synchronous_composite_for_testing_ = in;
  }

  // Called by Create() functions and subclasses to finish initialization.
  // |show_callback| will be invoked once WebWidgetClient::Show() occurs, and
  // should be null if Show() won't be triggered for this widget.
  void Init(ShowCallback show_callback, blink::WebWidget* web_widget);

  base::WeakPtr<RenderWidget> AsWeakPtr();

 protected:
  // An Init*() method must be called after creating a RenderWidget, which will
  // make the RenderWidget self-referencing. Then it can be deleted by calling
  // by calling OnClose().
  RenderWidget(int32_t widget_routing_id,
               CompositorDependencies* compositor_deps,
               const ScreenInfo& screen_info,
               blink::WebDisplayMode display_mode,
               bool is_frozen,
               bool hidden,
               bool never_visible,
               mojom::WidgetRequest widget_request);
  ~RenderWidget() override;

  // Close the underlying WebWidget and stop the compositor.
  virtual void Close();

  // Notify subclasses that we initiated the paint operation.
  virtual void DidInitiatePaint() {}

 private:
  // Friend RefCounted so that the dtor can be non-public. Using this class
  // without ref-counting is an error.
  friend class base::RefCounted<RenderWidget>;

  // TODO(nasko): Temporarily friend RenderFrameImpl for WasSwappedOut(),
  // while we move frame specific code away from RenderViewImpl/RenderWidget.
  friend class RenderFrameImpl;

  // For unit tests.
  friend class InteractiveRenderWidget;
  friend class PopupRenderWidget;
  friend class QueueMessageSwapPromiseTest;
  friend class RenderWidgetTest;
  friend class RenderViewImplTest;  // TODO(ajwong): Can this be removed?
  FRIEND_TEST_ALL_PREFIXES(RenderWidgetPopupUnittest, EmulatingPopupRect);

  static scoped_refptr<base::SingleThreadTaskRunner> GetCleanupTaskRunner();

  // Creates the compositor, but leaves it in a stopped state, where it will
  // not set up IPC channels or begin trying to produce frames until started
  // via StartStopCompositor().
  LayerTreeView* InitializeLayerTreeView();

  // If appropriate, initiates the compositor to set up IPC channels and begin
  // its scheduler. Otherwise, pauses the scheduler and tears down its IPC
  // channels.
  void StartStopCompositor();

  // Request the window to close from the renderer by sending the request to the
  // browser.
  void DoDeferredClose();

  gfx::Size GetSizeForWebWidget() const;
  void ResizeWebWidget();

  // Helper method to get the device_viewport_size() from the compositor, which
  // is always in physical pixels.
  gfx::Size CompositorViewportSize() const;

  // Just Close the WebWidget, in cases where the Close() will be deferred.
  // It is safe to call this multiple times, which happens in the case of
  // frame widgets beings closed, since subsequent calls are ignored.
  void CloseWebWidget();

#if BUILDFLAG(USE_EXTERNAL_POPUP_MENU)
  void SetExternalPopupOriginAdjustmentsForEmulation(ExternalPopupMenu* popup);
#endif

  // RenderWidget IPC message handlers.
  void OnHandleInputEvent(
      const blink::WebInputEvent* event,
      const std::vector<const blink::WebInputEvent*>& coalesced_events,
      const ui::LatencyInfo& latency_info,
      InputEventDispatchType dispatch_type);
  void OnClose();
  void OnCreatingNewAck();
  void OnEnableDeviceEmulation(const blink::WebDeviceEmulationParams& params);
  void OnDisableDeviceEmulation();
  void OnWasHidden();
  void OnWasShown(base::TimeTicks show_request_timestamp,
                  bool was_evicted,
                  const base::Optional<content::RecordTabSwitchTimeRequest>&
                      record_tab_switch_time_request);
  void OnCreateVideoAck(int32_t video_id);
  void OnUpdateVideoAck(int32_t video_id);
  void OnRequestSetBoundsAck();
  void OnForceRedraw(int snapshot_id);
  void OnShowContextMenu(ui::MenuSourceType source_type,
                         const gfx::Point& location);

  void OnSetTextDirection(blink::WebTextDirection direction);
  void OnGetFPS();
  void OnUpdateScreenRects(const gfx::Rect& widget_screen_rect,
                           const gfx::Rect& window_screen_rect);
  void OnSetViewportIntersection(const gfx::Rect& viewport_intersection,
                                 const gfx::Rect& compositor_visible_rect,
                                 blink::FrameOcclusionState occlusion_state);
  void OnSetIsInert(bool);
  void OnSetInheritedEffectiveTouchAction(cc::TouchAction touch_action);
  void OnUpdateRenderThrottlingStatus(bool is_throttled,
                                      bool subtree_throttled);
  void OnDragTargetDragEnter(
      const std::vector<DropData::Metadata>& drop_meta_data,
      const gfx::PointF& client_pt,
      const gfx::PointF& screen_pt,
      blink::WebDragOperationsMask operations_allowed,
      int key_modifiers);
  void OnDragTargetDragOver(const gfx::PointF& client_pt,
                            const gfx::PointF& screen_pt,
                            blink::WebDragOperationsMask operations_allowed,
                            int key_modifiers);
  void OnDragTargetDragLeave(const gfx::PointF& client_point,
                             const gfx::PointF& screen_point);
  void OnDragTargetDrop(const DropData& drop_data,
                        const gfx::PointF& client_pt,
                        const gfx::PointF& screen_pt,
                        int key_modifiers);
  void OnDragSourceEnded(const gfx::PointF& client_point,
                         const gfx::PointF& screen_point,
                         blink::WebDragOperation drag_operation);
  void OnDragSourceSystemDragEnded();
  void OnOrientationChange();
  void OnWaitNextFrameForTests(int routing_id);

  // Sets the "hidden" state of this widget.  All modification of is_hidden_
  // should use this method so that we can properly inform the RenderThread of
  // our state.
  void SetHidden(bool hidden);

  // Sets the fullscreen state for the WebView.
  // TODO(danakj): This is currently located on RenderWidget but is a page/view
  // state, and should move to RenderView.
  void SetIsFullscreen(bool fullscreen);

  // Returns a rect that the compositor needs to raster. For a main frame this
  // is always the entire viewport, but for out-of-process iframes this can be
  // constrained to limit overdraw.
  gfx::Rect ViewportVisibleRect();

  // QueueMessage implementation extracted into a static method for easy
  // testing.
  static std::unique_ptr<cc::SwapPromise> QueueMessageImpl(
      IPC::Message* msg,
      FrameSwapMessageQueue* frame_swap_message_queue,
      scoped_refptr<IPC::SyncMessageFilter> sync_message_filter,
      int source_frame_number);

  // Returns the range of the text that is being composed or the selection if
  // the composition does not exist.
  void GetCompositionRange(gfx::Range* range);

  // Returns true if the composition range or composition character bounds
  // should be sent to the browser process.
  bool ShouldUpdateCompositionInfo(
      const gfx::Range& range,
      const std::vector<gfx::Rect>& bounds);

  // Override point to obtain that the current input method state about
  // composition text.
  bool CanComposeInline();

  // Set the pending window rect.
  // Because the real render_widget is hosted in another process, there is
  // a time period where we may have set a new window rect which has not yet
  // been processed by the browser.  So we maintain a pending window rect
  // size.  If JS code sets the WindowRect, and then immediately calls
  // GetWindowRect() we'll use this pending window rect as the size.
  void SetPendingWindowRect(const blink::WebRect& r);

  // Returns the WebFrameWidget associated with this RenderWidget if any.
  // Returns nullptr if GetWebWidget() returns nullptr or returns a WebWidget
  // that is not a WebFrameWidget. A WebFrameWidget only makes sense when there
  // a local root associated with it. RenderWidgetFullscreenPepper and a swapped
  // out RenderWidgets are amongst the cases where this method returns nullptr.
  blink::WebFrameWidget* GetFrameWidget() const;

  // Applies/Removes the DevTools device emulation transformation to/from a
  // window rect.
  void ScreenRectToEmulatedIfNeeded(blink::WebRect* window_rect) const;
  void EmulatedToScreenRectIfNeeded(blink::WebRect* window_rect) const;

  void UpdateSurfaceAndScreenInfo(
      const viz::LocalSurfaceIdAllocation& new_local_surface_id_allocation,
      const gfx::Size& compositor_viewport_pixel_size,
      const ScreenInfo& new_screen_info);

  // Used to force the size of a window when running web tests.
  void SetWindowRectSynchronously(const gfx::Rect& new_window_rect);

  void UpdateCaptureSequenceNumber(uint32_t capture_sequence_number);

  // A variant of Send but is fatal if it fails. The browser may
  // be waiting for this IPC Message and if the send fails the browser will
  // be left in a state waiting for something that never comes. And if it
  // never comes then it may later determine this is a hung renderer; so
  // instead fail right away.
  void SendOrCrash(IPC::Message* msg);

  // Determines whether or not RenderWidget should process IME events from the
  // browser. It always returns true unless there is no WebFrameWidget to
  // handle the event, or there is no page focus.
  bool ShouldHandleImeEvents() const;

  void UpdateTextInputStateInternal(bool show_virtual_keyboard,
                                    bool reply_to_request);

  gfx::ColorSpace GetRasterColorSpace() const;

  void UpdateZoom(double zoom_level);

#if BUILDFLAG(ENABLE_PLUGINS)
  // Returns the focused pepper plugin, if any, inside the WebWidget. That is
  // the pepper plugin which is focused inside a frame which belongs to the
  // local root associated with this RenderWidget.
  PepperPluginInstanceImpl* GetFocusedPepperPluginInsideWidget();
#endif
  void RecordTimeToFirstActivePaint();

  // This method returns the WebLocalFrame which is currently focused and
  // belongs to the frame tree associated with this RenderWidget.
  blink::WebLocalFrame* GetFocusedWebLocalFrameInWidget() const;

  // Called with the resulting frame sink from WarmupCompositor() since frame
  // sink creation can be asynchronous.
  void OnReplyForWarmupCompositor(std::unique_ptr<cc::LayerTreeFrameSink> sink);

  // Common code shared to execute the creation of a LayerTreeFrameSink, shared
  // by the warmup and standard request paths. Callers should verify they really
  // want to do this before calling it as this method does no verification.
  void DoRequestNewLayerTreeFrameSink(LayerTreeFrameSinkCallback callback);

  // Whether this widget is for a frame. This excludes widgets that are not for
  // a frame (eg popups, pepper), but includes both the main frame
  // (via delegate_) and subframes (via for_child_local_root_frame_).
  bool for_frame() const { return delegate_ || for_child_local_root_frame_; }

  // Routing ID that allows us to communicate to the parent browser process
  // RenderWidgetHost.
  const int32_t routing_id_;

  // Dependencies for initializing a compositor, including flags for optional
  // features.
  CompositorDependencies* const compositor_deps_;

  // Use GetWebWidget() instead of using webwidget_internal_ directly.
  // We are responsible for destroying this object via its Close method.
  // May be NULL when the window is closing.
  blink::WebWidget* webwidget_internal_;

  // The delegate for this object which is just a RenderViewImpl.
  std::unique_ptr<RenderWidgetDelegate> delegate_;

  // This is lazily constructed and must not outlive webwidget_.
  std::unique_ptr<LayerTreeView> layer_tree_view_;

  // The rect where this view should be initially shown.
  gfx::Rect initial_rect_;

  // Web tests override the device scale factor in the renderer with this. We
  // store it to keep the override if the browser passes along VisualProperties
  // with the real device scale factor. A value of 0.f means this is ignored.
  float device_scale_factor_for_testing_ = 0.f;

  // The size of the RenderWidget in DIPs. This may differ from the viewport
  // set in the compositor, as the viewport can be a subset of the RenderWidget
  // in such cases as:
  // - When (hiding-on-scroll) top and bottom controls are present.
  // - Rounding issues with OOPIFs (??).
  gfx::Size size_;

  // The size of the visible viewport in pixels.
  gfx::Size visible_viewport_size_;

  // Whether the WebWidget is in auto resize mode, which is used for example
  // by extension popups.
  bool auto_resize_mode_;

  // The minimum size to use for auto-resize.
  gfx::Size min_size_for_auto_resize_;

  // The maximum size to use for auto-resize.
  gfx::Size max_size_for_auto_resize_;

  // Indicates that we shouldn't bother generated paint events.
  bool is_hidden_;

  // Indicates that we are never visible, so never produce graphical output.
  const bool compositor_never_visible_;

  // Indicates whether tab-initiated fullscreen was granted.
  bool is_fullscreen_granted_;

  // Indicates the display mode.
  blink::WebDisplayMode display_mode_;

  // It is possible that one ImeEventGuard is nested inside another
  // ImeEventGuard. We keep track of the outermost one, and update it as needed.
  ImeEventGuard* ime_event_guard_;

  bool closed_ = false;
  // True if we have requested this widget be closed.  No more messages will
  // be sent, except for a Close.
  bool closing_ = false;

  // True if it is known that the host is in the process of being shut down.
  bool host_will_close_this_ = false;

  // A RenderWidget is frozen if it is the RenderWidget attached to the
  // RenderViewImpl for its main frame, but there is a proxy main frame in
  // RenderViewImpl's frame tree. Since proxy frames do not have content they
  // do not need a RenderWidget.
  // This flag should never be used for RenderWidgets attached to subframes, as
  // those RenderWidgets are able to be created/deleted along with the frames,
  // unlike the main frame RenderWidget (for now).
  // TODO(419087): In this case the RenderWidget should not exist at all as
  // it has nothing to display, but since we can't destroy it without destroying
  // the RenderViewImpl, we freeze it instead.
  bool is_frozen_;

  // In web tests, synchronous resizing mode may be used. Normally each widget's
  // size is controlled by IPC from the browser. In synchronous resize mode the
  // renderer controls the size directly, and IPCs from the browser must be
  // ignored. This was deprecated but then later undeprecated, so it is now
  // called unfortunate instead. See https://crbug.com/309760. When this is
  // enabled the various size properties will be controlled directly when
  // SetWindowRect() is called instead of needing a round trip through the
  // browser.
  // Note that SetWindowRectSynchronouslyForTesting() provides a secondary way
  // to control the size of the RenderWidget independently from the renderer
  // process, without the use of this mode, however it would be overridden by
  // the browser if they disagree.
  bool synchronous_resize_mode_for_testing_ = false;
  // In web tests, synchronous composites should not be nested inside another
  // composite, and this bool is used to guard against that.
  bool in_synchronous_composite_for_testing_ = false;

  // Stores information about the current text input.
  blink::WebTextInputInfo text_input_info_;

  // Stores the current text input type of |webwidget_|.
  ui::TextInputType text_input_type_;

  // Stores the current text input mode of |webwidget_|.
  ui::TextInputMode text_input_mode_;

  // Stores the current text input flags of |webwidget_|.
  int text_input_flags_;

  // Indicates whether currently focused input field has next/previous focusable
  // form input field.
  int next_previous_flags_;

  // Stores the current type of composition text rendering of |webwidget_|.
  bool can_compose_inline_;

  // Stores the current selection bounds.
  gfx::Rect selection_focus_rect_;
  gfx::Rect selection_anchor_rect_;

  // Stores the current composition character bounds.
  std::vector<gfx::Rect> composition_character_bounds_;

  // Stores the current composition range.
  gfx::Range composition_range_;

  // While we are waiting for the browser to update window sizes, we track the
  // pending size temporarily.
  int pending_window_rect_count_;
  gfx::Rect pending_window_rect_;

  // The screen rects of the view and the window that contains it.
  gfx::Rect widget_screen_rect_;
  gfx::Rect window_screen_rect_;

  scoped_refptr<WidgetInputHandlerManager> widget_input_handler_manager_;

  std::unique_ptr<RenderWidgetInputHandler> input_handler_;

  // The time spent in input handlers this frame. Used to throttle input acks.
  base::TimeDelta total_input_handling_time_this_frame_;

  // Properties of the screen hosting this RenderWidget instance.
  ScreenInfo screen_info_;

  // True if the IME requests updated composition info.
  bool monitor_composition_info_;

  std::unique_ptr<RenderWidgetScreenMetricsEmulator> screen_metrics_emulator_;

  // Popups may be displaced when screen metrics emulation is enabled.
  // These values are used to properly adjust popup position.
  gfx::Point popup_view_origin_for_emulation_;
  gfx::Point popup_screen_origin_for_emulation_;
  float popup_origin_scale_for_emulation_;

  scoped_refptr<FrameSwapMessageQueue> frame_swap_message_queue_;

  // Lists of RenderFrameProxy objects that need to be notified of
  // compositing-related events (e.g. DidCommitCompositorFrame).
  base::ObserverList<RenderFrameProxy>::Unchecked render_frame_proxies_;

  // A list of RenderFrames associated with this RenderWidget. Notifications
  // are sent to each frame in the list for events such as changing
  // visibility state for example.
  base::ObserverList<RenderFrameImpl>::Unchecked render_frames_;

  base::ObserverList<BrowserPlugin>::Unchecked browser_plugins_;

  bool has_host_context_menu_location_;
  gfx::Point host_context_menu_location_;

  std::unique_ptr<blink::scheduler::WebRenderWidgetSchedulingState>
      render_widget_scheduling_state_;

  // Mouse Lock dispatcher attached to this view.
  std::unique_ptr<RenderWidgetMouseLockDispatcher> mouse_lock_dispatcher_;

  // Wraps the |webwidget_| as a MouseLockDispatcher::LockTarget interface.
  std::unique_ptr<MouseLockDispatcher::LockTarget> webwidget_mouse_lock_target_;

  // Set to true while a warmup is in progress. Set to false if the warmup is
  // completed or aborted. If aborted, the reply callback is also cancelled by
  // invalidating the |warmup_weak_ptr_factory_|.
  bool warmup_frame_sink_request_pending_ = false;
  // Set after warmup completes without being aborted. This frame sink will be
  // returned on the next request for a frame sink instead of creating a new
  // one.
  std::unique_ptr<cc::LayerTreeFrameSink> warmup_frame_sink_;
  // Set if a request for a frame sink arrives while a warmup is in progress.
  // Then this stores the request to be satisfied once the warmup completes.
  LayerTreeFrameSinkCallback after_warmup_callback_;

  viz::LocalSurfaceIdAllocation local_surface_id_allocation_from_parent_;

  // Indicates whether this widget has focus.
  bool has_focus_;

  // Whether this widget is for a child local root frame. This excludes widgets
  // that are not for a frame (eg popups) and excludes the widget for the main
  // frame (which is attached to the RenderViewImpl).
  bool for_child_local_root_frame_;

  // A callback into the creator/opener of this widget, to be executed when
  // WebWidgetClient::Show() occurs.
  ShowCallback show_callback_;

#if defined(OS_MACOSX)
  // Responds to IPCs from TextInputClientMac regarding getting string at given
  // position or range as well as finding character index at a given position.
  std::unique_ptr<TextInputClientObserver> text_input_client_observer_;
#endif

  // Stores edit commands associated to the next key event.
  // Will be cleared as soon as the next key event is processed.
  EditCommands edit_commands_;

  // This field stores drag/drop related info for the event that is currently
  // being handled. If the current event results in starting a drag/drop
  // session, this info is sent to the browser along with other drag/drop info.
  DragEventSourceInfo possible_drag_event_info_;

  bool first_update_visual_state_after_hidden_;
  base::TimeTicks was_shown_time_;

  // Object to record tab switch time into this RenderWidget
  TabSwitchTimeRecorder tab_switch_time_recorder_;

  // Whether or not Blink's viewport size should be shrunk by the height of the
  // URL-bar.
  bool browser_controls_shrink_blink_size_ = false;
  // The height of the browser top controls.
  float top_controls_height_ = 0.f;
  // The height of the browser bottom controls.
  float bottom_controls_height_ = 0.f;

  // The last seen page scale state, which comes from the main frame and is
  // propagated through the RenderWidget tree. This state is passed to any new
  // child RenderWidget.
  float page_scale_factor_from_mainframe_ = 1.f;
  bool is_pinch_gesture_active_from_mainframe_ = false;

  scoped_refptr<MainThreadEventQueue> input_event_queue_;

  mojo::Binding<mojom::Widget> widget_binding_;

  gfx::Rect compositor_visible_rect_;

  // Different consumers in the browser process makes different assumptions, so
  // must always send the first IPC regardless of value.
  base::Optional<bool> has_touch_handlers_;

  uint32_t last_capture_sequence_number_ = 0u;

  // Used to generate a callback for the reply when making the warmup frame
  // sink, and to cancel that callback if the warmup is aborted.
  base::WeakPtrFactory<RenderWidget> warmup_weak_ptr_factory_{this};
  // This factory is invalidated when the WebWidget is closed.
  base::WeakPtrFactory<RenderWidget> close_weak_ptr_factory_{this};
  // This factory is invalidated when the RenderWidget is destroyed.
  base::WeakPtrFactory<RenderWidget> weak_ptr_factory_{this};

  DISALLOW_COPY_AND_ASSIGN(RenderWidget);
};

}  // namespace content

#endif  // CONTENT_RENDERER_RENDER_WIDGET_H_
