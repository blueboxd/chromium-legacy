// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_VIEWS_WIDGET_DESKTOP_AURA_DESKTOP_WINDOW_TREE_HOST_X11_H_
#define UI_VIEWS_WIDGET_DESKTOP_AURA_DESKTOP_WINDOW_TREE_HOST_X11_H_

#include <stddef.h>
#include <stdint.h>

#include <list>
#include <memory>
#include <set>
#include <string>
#include <vector>

#include "base/containers/flat_set.h"
#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "base/observer_list.h"
#include "ui/aura/scoped_window_targeter.h"
#include "ui/aura/window_tree_host.h"
#include "ui/base/x/x11_window.h"
#include "ui/events/platform/platform_event_dispatcher.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/geometry/size.h"
#include "ui/gfx/x/x11_types.h"
#include "ui/views/views_export.h"
#include "ui/views/widget/desktop_aura/desktop_window_tree_host.h"

namespace gfx {
class ImageSkia;
}

namespace ui {
enum class DomCode;
class EventHandler;
class KeyboardHook;
class KeyEvent;
class MouseEvent;
class TouchEvent;
class ScrollEvent;
}

namespace views {
class DesktopDragDropClientAuraX11;
class DesktopWindowTreeHostObserverX11;
class NonClientFrameView;
class X11DesktopWindowMoveClient;

class VIEWS_EXPORT DesktopWindowTreeHostX11
    : public DesktopWindowTreeHost,
      public aura::WindowTreeHost,
      public ui::PlatformEventDispatcher,
      public ui::XWindow::Delegate {
 public:
  DesktopWindowTreeHostX11(
      internal::NativeWidgetDelegate* native_widget_delegate,
      DesktopNativeWidgetAura* desktop_native_widget_aura);
  ~DesktopWindowTreeHostX11() override;

  // A way of converting an X11 |xid| host window into the content_window()
  // of the associated DesktopNativeWidgetAura.
  static aura::Window* GetContentWindowForXID(XID xid);

  // A way of converting an X11 |xid| host window into this object.
  static DesktopWindowTreeHostX11* GetHostForXID(XID xid);

  // Get all open top-level windows. This includes windows that may not be
  // visible. This list is sorted in their stacking order, i.e. the first window
  // is the topmost window.
  static std::vector<aura::Window*> GetAllOpenWindows();

  // Returns the current bounds in terms of the X11 Root Window.
  gfx::Rect GetX11RootWindowBounds() const;

  // Returns the current bounds in terms of the X11 Root Window including the
  // borders provided by the window manager (if any).
  gfx::Rect GetX11RootWindowOuterBounds() const;

  // Returns the window shape if the window is not rectangular. Returns NULL
  // otherwise.
  ::Region GetWindowShape() const;

  void AddObserver(DesktopWindowTreeHostObserverX11* observer);
  void RemoveObserver(DesktopWindowTreeHostObserverX11* observer);

  // Swaps the current handler for events in the non client view with |handler|.
  void SwapNonClientEventHandler(std::unique_ptr<ui::EventHandler> handler);

  // Runs the |func| callback for each content-window, and deallocates the
  // internal list of open windows.
  static void CleanUpWindowList(void (*func)(aura::Window* window));

  // Disables event listening to make |dialog| modal.
  base::OnceClosure DisableEventListening();

  // Returns a map of KeyboardEvent code to KeyboardEvent key values.
  base::flat_map<std::string, std::string> GetKeyboardLayoutMap() override;

  // This must be called before the window is created, because the visual cannot
  // be changed after.
  void SetVisualId(VisualID visual_id);

 protected:
  // Overridden from DesktopWindowTreeHost:
  void Init(const Widget::InitParams& params) override;
  void OnNativeWidgetCreated(const Widget::InitParams& params) override;
  void OnWidgetInitDone() override;
  void OnActiveWindowChanged(bool active) override;
  std::unique_ptr<corewm::Tooltip> CreateTooltip() override;
  std::unique_ptr<aura::client::DragDropClient> CreateDragDropClient(
      DesktopNativeCursorManager* cursor_manager) override;
  void Close() override;
  void CloseNow() override;
  aura::WindowTreeHost* AsWindowTreeHost() override;
  void Show(ui::WindowShowState show_state,
            const gfx::Rect& restore_bounds) override;
  bool IsVisible() const override;
  void SetSize(const gfx::Size& requested_size) override;
  void StackAbove(aura::Window* window) override;
  void StackAtTop() override;
  void CenterWindow(const gfx::Size& size) override;
  void GetWindowPlacement(gfx::Rect* bounds,
                          ui::WindowShowState* show_state) const override;
  gfx::Rect GetWindowBoundsInScreen() const override;
  gfx::Rect GetClientAreaBoundsInScreen() const override;
  gfx::Rect GetRestoredBounds() const override;
  std::string GetWorkspace() const override;
  gfx::Rect GetWorkAreaBoundsInScreen() const override;
  void SetShape(std::unique_ptr<Widget::ShapeRects> native_shape) override;
  void Activate() override;
  void Deactivate() override;
  bool IsActive() const override;
  void Maximize() override;
  void Minimize() override;
  void Restore() override;
  bool IsMaximized() const override;
  bool IsMinimized() const override;
  bool HasCapture() const override;
  void SetZOrderLevel(ui::ZOrderLevel order) override;
  ui::ZOrderLevel GetZOrderLevel() const override;
  void SetVisibleOnAllWorkspaces(bool always_visible) override;
  bool IsVisibleOnAllWorkspaces() const override;
  bool SetWindowTitle(const base::string16& title) override;
  void ClearNativeFocus() override;
  Widget::MoveLoopResult RunMoveLoop(
      const gfx::Vector2d& drag_offset,
      Widget::MoveLoopSource source,
      Widget::MoveLoopEscapeBehavior escape_behavior) override;
  void EndMoveLoop() override;
  void SetVisibilityChangedAnimationsEnabled(bool value) override;
  NonClientFrameView* CreateNonClientFrameView() override;
  bool ShouldUseNativeFrame() const override;
  bool ShouldWindowContentsBeTransparent() const override;
  void FrameTypeChanged() override;
  void SetFullscreen(bool fullscreen) override;
  bool IsFullscreen() const override;
  void SetOpacity(float opacity) override;
  void SetAspectRatio(const gfx::SizeF& aspect_ratio) override;
  void SetWindowIcons(const gfx::ImageSkia& window_icon,
                      const gfx::ImageSkia& app_icon) override;
  void InitModalType(ui::ModalType modal_type) override;
  void FlashFrame(bool flash_frame) override;
  bool IsAnimatingClosed() const override;
  bool IsTranslucentWindowOpacitySupported() const override;
  void SizeConstraintsChanged() override;
  bool ShouldUpdateWindowTransparency() const override;
  bool ShouldUseDesktopNativeCursorManager() const override;
  bool ShouldCreateVisibilityController() const override;

  // Overridden from aura::WindowTreeHost:
  gfx::Transform GetRootTransform() const override;
  ui::EventSource* GetEventSource() override;
  gfx::AcceleratedWidget GetAcceleratedWidget() override;
  void ShowImpl() override;
  void HideImpl() override;
  gfx::Rect GetBoundsInPixels() const override;
  void SetBoundsInPixels(const gfx::Rect& requested_bounds_in_pixels) override;
  gfx::Point GetLocationOnScreenInPixels() const override;
  void SetCapture() override;
  void ReleaseCapture() override;
  bool CaptureSystemKeyEventsImpl(
      base::Optional<base::flat_set<ui::DomCode>> dom_codes) override;
  void ReleaseSystemKeyEventCapture() override;
  bool IsKeyLocked(ui::DomCode dom_code) override;
  void SetCursorNative(gfx::NativeCursor cursor) override;
  void MoveCursorToScreenLocationInPixels(
      const gfx::Point& location_in_pixels) override;
  void OnCursorVisibilityChangedNative(bool show) override;

  // Overridden from display::DisplayObserver via aura::WindowTreeHost:
  void OnDisplayMetricsChanged(const display::Display& display,
                               uint32_t changed_metrics) override;

  // Overridden from ui::XWindow::Delegate
  void OnXWindowCreated() override;
  void OnXWindowMapped() override;
  void OnXWindowUnmapped() override;
  void OnXWindowStateChanged() override;
  void OnXWindowWorkspaceChanged() override;
  void OnXWindowKeyEvent(ui::KeyEvent* key_event) override;
  void OnXWindowMouseEvent(ui::MouseEvent* mouseev) override;
  void OnXWindowTouchEvent(ui::TouchEvent* touch_event) override;
  void OnXWindowScrollEvent(ui::ScrollEvent* scroll_event) override;
  void OnXWindowSelectionEvent(XEvent* xev) override;
  void OnXWindowDragDropEvent(XEvent* xev) override;
  void OnXWindowChildCrossingEvent(XEvent* xev) override;
  void OnXWindowRawKeyEvent(XEvent* xev) override;
  void OnXWindowSizeChanged(const gfx::Size& size) override;
  void OnXWindowCloseRequested() override;
  void OnXWindowMoved(const gfx::Point& window_origin) override;
  void OnXWindowDamageEvent(const gfx::Rect& damage_rect) override;
  void OnXWindowLostPointerGrab() override;
  void OnXWindowLostCapture() override;
  void OnXWindowIsActiveChanged(bool active) override;
  gfx::Size GetMinimumSizeForXWindow() override;
  gfx::Size GetMaximumSizeForXWindow() override;

 private:
  friend class DesktopWindowTreeHostX11HighDPITest;

  // Initializes our X11 surface to draw on. This method performs all
  // initialization related to talking to the X11 server.
  void InitX11Window(const Widget::InitParams& params);

  // Creates an aura::WindowEventDispatcher to contain the content_window()
  // along with all aura client objects that direct behavior.
  aura::WindowEventDispatcher* InitDispatcher(const Widget::InitParams& params);

  // Adjusts |requested_size| to avoid the WM "feature" where setting the
  // window size to the monitor size causes the WM to set the EWMH for
  // fullscreen.
  gfx::Size AdjustSize(const gfx::Size& requested_size);

  // Sets whether the window's borders are provided by the window manager.
  void SetUseNativeFrame(bool use_native_frame);

  // Dispatches a mouse event, taking mouse capture into account. If a
  // different host has capture, we translate the event to its coordinate space
  // and dispatch it to that host instead.
  void DispatchMouseEvent(ui::MouseEvent* event);

  // Dispatches a touch event, taking capture into account. If a different host
  // has capture, then touch-press events are translated to its coordinate space
  // and dispatched to that host instead.
  void DispatchTouchEvent(ui::TouchEvent* event);

  // Dispatches a key event.
  void DispatchKeyEvent(ui::KeyEvent* event);

  // Resets the window region for the current widget bounds if necessary.
  void ResetWindowRegion();

  // See comment for variable open_windows_.
  static std::list<XID>& open_windows();

  // Map the window (shows it) taking into account the given |show_state|.
  void MapWindow(ui::WindowShowState show_state);

  void SetWindowTransparency();

  // Relayout the widget's client and non-client views.
  void Relayout();

  // ui::PlatformEventDispatcher:
  bool CanDispatchEvent(const ui::PlatformEvent& event) override;
  uint32_t DispatchEvent(const ui::PlatformEvent& event) override;

  void DelayedChangeFrameType(Widget::FrameType new_type);

  gfx::Rect ToDIPRect(const gfx::Rect& rect_in_pixels) const;
  gfx::Rect ToPixelRect(const gfx::Rect& rect_in_dip) const;

  // Enables event listening after closing |dialog|.
  void EnableEventListening();

  // Set visibility and fire OnNativeWidgetVisibilityChanged() if it changed.
  void SetVisible(bool visible);

  // Accessor for DesktopNativeWidgetAura::content_window().
  aura::Window* content_window();

  // Callback for a swapbuffer after resize.
  void OnCompleteSwapWithNewSize(const gfx::Size& size);

  // The bounds of our window before we were maximized.
  gfx::Rect restored_bounds_in_pixels_;

  // Whether |xwindow_| was requested to be fullscreen via SetFullscreen().
  bool is_fullscreen_ = false;

  // The z-order level of the window; the window exhibits "always on top"
  // behavior if > 0.
  ui::ZOrderLevel z_order_ = ui::ZOrderLevel::kNormal;

  DesktopDragDropClientAuraX11* drag_drop_client_ = nullptr;

  std::unique_ptr<ui::EventHandler> x11_non_client_event_filter_;
  std::unique_ptr<X11DesktopWindowMoveClient> x11_window_move_client_;

  // TODO(beng): Consider providing an interface to DesktopNativeWidgetAura
  //             instead of providing this route back to Widget.
  internal::NativeWidgetDelegate* native_widget_delegate_;

  DesktopNativeWidgetAura* desktop_native_widget_aura_;

  // We can optionally have a parent which can order us to close, or own
  // children who we're responsible for closing when we CloseNow().
  DesktopWindowTreeHostX11* window_parent_ = nullptr;
  std::set<DesktopWindowTreeHostX11*> window_children_;

  base::ObserverList<DesktopWindowTreeHostObserverX11>::Unchecked
      observer_list_;

  // The current DesktopWindowTreeHostX11 which has capture. Set synchronously
  // when capture is requested via SetCapture().
  static DesktopWindowTreeHostX11* g_current_capture;

  // A list of all (top-level) windows that have been created but not yet
  // destroyed.
  static std::list<XID>* open_windows_;

  // Cached value for SetVisible.  Not the same as the IsVisible public API.
  bool is_compositor_set_visible_ = false;

  // Captures system key events when keyboard lock is requested.
  std::unique_ptr<ui::KeyboardHook> keyboard_hook_;

  std::unique_ptr<aura::ScopedWindowTargeter> targeter_for_modal_;

  uint32_t modal_dialog_counter_ = 0;

  std::unique_ptr<CompositorObserver> compositor_observer_;

  std::unique_ptr<ui::XWindow> x11_window_;

  // The display and the native X window hosting the root window.
  base::WeakPtrFactory<DesktopWindowTreeHostX11> close_widget_factory_{this};
  base::WeakPtrFactory<DesktopWindowTreeHostX11> weak_factory_{this};

  DISALLOW_COPY_AND_ASSIGN(DesktopWindowTreeHostX11);
};

}  // namespace views

#endif  // UI_VIEWS_WIDGET_DESKTOP_AURA_DESKTOP_WINDOW_TREE_HOST_X11_H_
