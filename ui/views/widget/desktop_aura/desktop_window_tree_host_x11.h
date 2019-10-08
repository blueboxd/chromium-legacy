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
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/geometry/size.h"
#include "ui/gfx/x/x11_types.h"
#include "ui/platform_window/platform_window_delegate.h"
#include "ui/platform_window/x11/x11_window.h"
#include "ui/views/views_export.h"
#include "ui/views/widget/desktop_aura/desktop_window_tree_host_linux.h"

namespace gfx {
class ImageSkia;
}

namespace ui {
enum class DomCode;
class X11Window;
}  // namespace ui

namespace views {
class DesktopDragDropClientAuraX11;
class DesktopWindowTreeHostObserverX11;
class X11DesktopWindowMoveClient;

class VIEWS_EXPORT DesktopWindowTreeHostX11 : public DesktopWindowTreeHostLinux,
                                              public ui::XEventDelegate {
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

  // Runs the |func| callback for each content-window, and deallocates the
  // internal list of open windows.
  static void CleanUpWindowList(void (*func)(aura::Window* window));

  // Disables event listening to make |dialog| modal.
  base::OnceClosure DisableEventListening();

  // Returns a map of KeyboardEvent code to KeyboardEvent key values.
  base::flat_map<std::string, std::string> GetKeyboardLayoutMap() override;

 protected:
  // Overridden from DesktopWindowTreeHost:
  void Init(const Widget::InitParams& params) override;
  void OnNativeWidgetCreated(const Widget::InitParams& params) override;
  std::unique_ptr<aura::client::DragDropClient> CreateDragDropClient(
      DesktopNativeCursorManager* cursor_manager) override;
  void SetShape(std::unique_ptr<Widget::ShapeRects> native_shape) override;
  Widget::MoveLoopResult RunMoveLoop(
      const gfx::Vector2d& drag_offset,
      Widget::MoveLoopSource source,
      Widget::MoveLoopEscapeBehavior escape_behavior) override;
  void EndMoveLoop() override;
  void SetOpacity(float opacity) override;
  void SetAspectRatio(const gfx::SizeF& aspect_ratio) override;
  void SetWindowIcons(const gfx::ImageSkia& window_icon,
                      const gfx::ImageSkia& app_icon) override;
  void InitModalType(ui::ModalType modal_type) override;
  bool IsAnimatingClosed() const override;
  bool IsTranslucentWindowOpacitySupported() const override;
  void SizeConstraintsChanged() override;
  bool ShouldUseDesktopNativeCursorManager() const override;
  bool ShouldCreateVisibilityController() const override;

 private:
  friend class DesktopWindowTreeHostX11HighDPITest;

  // See comment for variable open_windows_.
  static std::list<XID>& open_windows();

  // Enables event listening after closing |dialog|.
  void EnableEventListening();

  // Callback for a swapbuffer after resize.
  void OnCompleteSwapWithNewSize(const gfx::Size& size) override;

  // PlatformWindowDelegate overrides:
  //
  // DWTHX11 temporarily overrides the PlatformWindowDelegate methods instead of
  // underlying DWTHPlatform and WTHPlatform. Eventually, these will be removed
  // from here as we progress in https://crbug.com/990756.
  void OnClosed() override;
  void OnAcceleratedWidgetAvailable(gfx::AcceleratedWidget widget) override;
  void OnAcceleratedWidgetDestroyed() override;
  void OnActivationChanged(bool active) override;
  void OnXWindowMapped() override;
  void OnXWindowUnmapped() override;
  void OnLostMouseGrab() override;

  // Overridden from ui::XEventDelegate.
  void OnXWindowSelectionEvent(XEvent* xev) override;
  void OnXWindowDragDropEvent(XEvent* xev) override;
  void OnXWindowRawKeyEvent(XEvent* xev) override;

  // Casts PlatformWindow into XWindow and returns the result. This is a temp
  // solution to access XWindow, which is subclassed by the X11Window, which is
  // PlatformWindow. This will be removed once we no longer to access XWindow
  // directly. See https://crbug.com/990756.
  ui::XWindow* GetXWindow();
  const ui::XWindow* GetXWindow() const;

  DesktopDragDropClientAuraX11* drag_drop_client_ = nullptr;

  std::unique_ptr<X11DesktopWindowMoveClient> x11_window_move_client_;

  base::ObserverList<DesktopWindowTreeHostObserverX11>::Unchecked
      observer_list_;

  // A list of all (top-level) windows that have been created but not yet
  // destroyed.
  static std::list<gfx::AcceleratedWidget>* open_windows_;

  std::unique_ptr<aura::ScopedWindowTargeter> targeter_for_modal_;

  uint32_t modal_dialog_counter_ = 0;

  // The display and the native X window hosting the root window.
  base::WeakPtrFactory<DesktopWindowTreeHostX11> weak_factory_{this};

  DISALLOW_COPY_AND_ASSIGN(DesktopWindowTreeHostX11);
};

}  // namespace views

#endif  // UI_VIEWS_WIDGET_DESKTOP_AURA_DESKTOP_WINDOW_TREE_HOST_X11_H_
