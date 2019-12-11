// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_OZONE_PLATFORM_WAYLAND_HOST_WAYLAND_WINDOW_H_
#define UI_OZONE_PLATFORM_WAYLAND_HOST_WAYLAND_WINDOW_H_

#include <memory>
#include <set>
#include <vector>

#include "base/callback.h"
#include "base/containers/flat_set.h"
#include "base/gtest_prod_util.h"
#include "base/memory/ref_counted.h"
#include "ui/events/platform/platform_event_dispatcher.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/native_widget_types.h"
#include "ui/ozone/platform/wayland/common/wayland_object.h"
#include "ui/platform_window/platform_window.h"
#include "ui/platform_window/platform_window_delegate.h"
#include "ui/platform_window/platform_window_init_properties.h"

namespace gfx {
class PointF;
}

namespace ui {

class BitmapCursorOzone;
class OSExchangeData;
class WaylandConnection;

class WaylandWindow : public PlatformWindow, public PlatformEventDispatcher {
 public:
  ~WaylandWindow() override;

  // A factory method that can create any of the derived types of WaylandWindow
  // (WaylandSurface, WaylandPopup and WaylandSubsurface).
  static std::unique_ptr<WaylandWindow> Create(
      PlatformWindowDelegate* delegate,
      WaylandConnection* connection,
      PlatformWindowInitProperties properties);

  static WaylandWindow* FromSurface(wl_surface* surface);

  // Updates the surface buffer scale of the window.  Top level windows take
  // scale from the output attached to either their current display or the
  // primary one if their widget is not yet created, children inherit scale from
  // their parent.  The method recalculates window bounds appropriately if asked
  // to do so (this is not needed upon window initialization).
  void UpdateBufferScale(bool update_bounds);

  wl_surface* surface() const { return surface_.get(); }

  void set_parent_window(WaylandWindow* parent_window) {
    parent_window_ = parent_window;
  }
  WaylandWindow* parent_window() const { return parent_window_; }

  gfx::AcceleratedWidget GetWidget() const;

  // Set whether this window has pointer focus and should dispatch mouse events.
  void SetPointerFocus(bool focus);
  bool has_pointer_focus() const { return has_pointer_focus_; }

  // Set whether this window has keyboard focus and should dispatch key events.
  void set_keyboard_focus(bool focus) { has_keyboard_focus_ = focus; }
  bool has_keyboard_focus() const { return has_keyboard_focus_; }

  // The methods set or return whether this window has touch focus and should
  // dispatch touch events.
  void set_touch_focus(bool focus) { has_touch_focus_ = focus; }
  bool has_touch_focus() const { return has_touch_focus_; }

  // Set a child of this window. It is very important in case of nested
  // shell_popups as long as they must be destroyed in the back order.
  void set_child_window(WaylandWindow* window) { child_window_ = window; }
  WaylandWindow* child_window() const { return child_window_; }

  // Set whether this window has an implicit grab (often referred to as capture
  // in Chrome code). Implicit grabs happen while a pointer is down.
  void set_has_implicit_grab(bool value) { has_implicit_grab_ = value; }
  bool has_implicit_grab() const { return has_implicit_grab_; }

  int32_t buffer_scale() const { return buffer_scale_; }
  int32_t ui_scale() const { return ui_scale_; }

  const base::flat_set<uint32_t>& entered_outputs_ids() const {
    return entered_outputs_ids_;
  }

  // Returns current type of the window.
  PlatformWindowType type() const { return type_; }

  // PlatformWindow
  void Show(bool inactive) override;
  void Hide() override;
  void Close() override;
  bool IsVisible() const override;
  void PrepareForShutdown() override;
  void SetBounds(const gfx::Rect& bounds) override;
  gfx::Rect GetBounds() override;
  void SetTitle(const base::string16& title) override;
  void SetCapture() override;
  void ReleaseCapture() override;
  bool HasCapture() const override;
  void ToggleFullscreen() override;
  void Maximize() override;
  void Minimize() override;
  void Restore() override;
  PlatformWindowState GetPlatformWindowState() const override;
  void Activate() override;
  void Deactivate() override;
  void SetUseNativeFrame(bool use_native_frame) override;
  bool ShouldUseNativeFrame() const override;
  void SetCursor(PlatformCursor cursor) override;
  void MoveCursorTo(const gfx::Point& location) override;
  void ConfineCursorToBounds(const gfx::Rect& bounds) override;
  void SetRestoredBoundsInPixels(const gfx::Rect& bounds) override;
  gfx::Rect GetRestoredBoundsInPixels() const override;
  bool ShouldWindowContentsBeTransparent() const override;
  void SetAspectRatio(const gfx::SizeF& aspect_ratio) override;
  void SetWindowIcons(const gfx::ImageSkia& window_icon,
                      const gfx::ImageSkia& app_icon) override;
  void SizeConstraintsChanged() override;

  // PlatformEventDispatcher
  bool CanDispatchEvent(const PlatformEvent& event) override;
  uint32_t DispatchEvent(const PlatformEvent& event) override;

  // Handles the configuration events coming from the shell objects.
  // The width and height come in DIP of the output that the surface is
  // currently bound to.
  virtual void HandleSurfaceConfigure(int32_t widht,
                                      int32_t height,
                                      bool is_maximized,
                                      bool is_fullscreen,
                                      bool is_activated);
  virtual void HandlePopupConfigure(const gfx::Rect& bounds);

  // Handles close requests.
  virtual void OnCloseRequest();

  // Notifies about drag/drop session events.
  virtual void OnDragEnter(const gfx::PointF& point,
                           std::unique_ptr<OSExchangeData> data,
                           int operation);
  virtual int OnDragMotion(const gfx::PointF& point,
                           uint32_t time,
                           int operation);
  virtual void OnDragDrop(std::unique_ptr<OSExchangeData> data);
  virtual void OnDragLeave();
  virtual void OnDragSessionClose(uint32_t dnd_action);

 protected:
  WaylandWindow(PlatformWindowDelegate* delegate,
                WaylandConnection* connection);

  WaylandConnection* connection() { return connection_; }
  PlatformWindowDelegate* delegate() { return delegate_; }

  // Sets bounds in dip.
  void SetBoundsDip(const gfx::Rect& bounds_dip);

  // Gets a parent window for this window.
  WaylandWindow* GetParentWindow(gfx::AcceleratedWidget parent_widget);

  // Sets the buffer scale.
  void SetBufferScale(int32_t scale, bool update_bounds);

  // Sets the ui scale.
  void set_ui_scale(int32_t ui_scale) { ui_scale_ = ui_scale; }

 private:
  FRIEND_TEST_ALL_PREFIXES(WaylandScreenTest, SetBufferScale);

  // Initializes the WaylandWindow with supplied properties.
  bool Initialize(PlatformWindowInitProperties properties);

  // Returns a root parent window.
  WaylandWindow* GetRootParentWindow();

  // Install a surface listener and start getting wl_output enter/leave events.
  void AddSurfaceListener();

  void AddEnteredOutputId(struct wl_output* output);
  void RemoveEnteredOutputId(struct wl_output* output);

  void UpdateCursorPositionFromEvent(std::unique_ptr<Event> event);

  WaylandWindow* GetTopLevelWindow();

  // It's important to set opaque region for opaque windows (provides
  // optimization hint for the Wayland compositor).
  void MaybeUpdateOpaqueRegion();

  bool IsOpaqueWindow() const;

  // Additional initialization of derived classes.
  virtual bool OnInitialize(PlatformWindowInitProperties properties) = 0;

  // wl_surface_listener
  static void Enter(void* data,
                    struct wl_surface* wl_surface,
                    struct wl_output* output);
  static void Leave(void* data,
                    struct wl_surface* wl_surface,
                    struct wl_output* output);

  PlatformWindowDelegate* delegate_;
  WaylandConnection* connection_;
  WaylandWindow* parent_window_ = nullptr;
  WaylandWindow* child_window_ = nullptr;

  wl::Object<wl_surface> surface_;

  // The current cursor bitmap (immutable).
  scoped_refptr<BitmapCursorOzone> bitmap_;

  // Current bounds of the platform window.
  gfx::Rect bounds_px_;
  // The bounds of the platform window before it went maximized or fullscreen.
  gfx::Rect restored_bounds_px_;

  bool has_pointer_focus_ = false;
  bool has_keyboard_focus_ = false;
  bool has_touch_focus_ = false;
  bool has_implicit_grab_ = false;
  // Wayland's scale factor for the output that this window currently belongs
  // to.
  int32_t buffer_scale_ = 1;
  // The UI scale may be forced through the command line, which means that it
  // replaces the default value that is equal to the natural device scale.
  // We need it to place and size the menus properly.
  float ui_scale_ = 1.0;

  // Stores current opacity of the window. Set on ::Initialize call.
  ui::PlatformWindowOpacity opacity_;

  // For top level window, stores IDs of outputs that the window is currently
  // rendered at.
  //
  // Not used by popups.  When sub-menus are hidden and shown again, Wayland
  // 'repositions' them to wrong outputs by sending them leave and enter
  // events so their list of entered outputs becomes meaningless after they have
  // been hidden at least once.  To determine which output the popup belongs to,
  // we ask its parent.
  base::flat_set<uint32_t> entered_outputs_ids_;

  // The type of the current WaylandWindow object.
  ui::PlatformWindowType type_ = ui::PlatformWindowType::kWindow;

  DISALLOW_COPY_AND_ASSIGN(WaylandWindow);
};

}  // namespace ui

#endif  // UI_OZONE_PLATFORM_WAYLAND_HOST_WAYLAND_WINDOW_H_
