// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/ozone/platform/wayland/host/wayland_window.h"

#include <wayland-client.h>

#include <memory>

#include "base/bind.h"
#include "ui/base/cursor/ozone/bitmap_cursor_factory_ozone.h"
#include "ui/events/event.h"
#include "ui/events/event_utils.h"
#include "ui/events/ozone/events_ozone.h"
#include "ui/events/platform/platform_event_source.h"
#include "ui/gfx/geometry/point_f.h"
#include "ui/ozone/platform/wayland/common/wayland_util.h"
#include "ui/ozone/platform/wayland/host/wayland_buffer_manager_host.h"
#include "ui/ozone/platform/wayland/host/wayland_connection.h"
#include "ui/ozone/platform/wayland/host/wayland_cursor_position.h"
#include "ui/ozone/platform/wayland/host/wayland_output_manager.h"
#include "ui/ozone/platform/wayland/host/wayland_pointer.h"

namespace ui {

WaylandWindow::WaylandWindow(PlatformWindowDelegate* delegate,
                             WaylandConnection* connection)
    : delegate_(delegate), connection_(connection) {}

WaylandWindow::~WaylandWindow() {
  PlatformEventSource::GetInstance()->RemovePlatformEventDispatcher(this);
  if (surface_)
    connection_->wayland_window_manager()->RemoveWindow(GetWidget());

  if (parent_window_)
    parent_window_->set_child_window(nullptr);
}

// static
WaylandWindow* WaylandWindow::FromSurface(wl_surface* surface) {
  if (!surface)
    return nullptr;
  return static_cast<WaylandWindow*>(
      wl_proxy_get_user_data(reinterpret_cast<wl_proxy*>(surface)));
}

void WaylandWindow::UpdateBufferScale(bool update_bounds) {
  DCHECK(connection_->wayland_output_manager());
  const auto* screen = connection_->wayland_output_manager()->wayland_screen();

  // The client might not create screen at all.
  if (!screen)
    return;

  const auto widget = GetWidget();

  int32_t new_scale = 0;
  if (parent_window_) {
    new_scale = parent_window_->buffer_scale_;
    ui_scale_ = parent_window_->ui_scale_;
  } else {
    const auto display = (widget == gfx::kNullAcceleratedWidget)
                             ? screen->GetPrimaryDisplay()
                             : screen->GetDisplayForAcceleratedWidget(widget);
    new_scale = connection_->wayland_output_manager()
                    ->GetOutput(display.id())
                    ->scale_factor();

    if (display::Display::HasForceDeviceScaleFactor())
      ui_scale_ = display::Display::GetForcedDeviceScaleFactor();
    else
      ui_scale_ = display.device_scale_factor();
  }
  SetBufferScale(new_scale, update_bounds);
}

gfx::AcceleratedWidget WaylandWindow::GetWidget() const {
  if (!surface_)
    return gfx::kNullAcceleratedWidget;
  return surface_.id();
}
void WaylandWindow::SetPointerFocus(bool focus) {
  has_pointer_focus_ = focus;

  // Whenever the window gets the pointer focus back, we must reinitialize the
  // cursor. Otherwise, it is invalidated whenever the pointer leaves the
  // surface and is not restored by the Wayland compositor.
  if (has_pointer_focus_ && bitmap_)
    connection_->SetCursorBitmap(bitmap_->bitmaps(), bitmap_->hotspot());
}

void WaylandWindow::Show(bool inactive) {
  NOTREACHED();
}

void WaylandWindow::Hide() {
  NOTREACHED();
}

void WaylandWindow::Close() {
  delegate_->OnClosed();
}

bool WaylandWindow::IsVisible() const {
  NOTREACHED();
  return false;
}

void WaylandWindow::PrepareForShutdown() {}

void WaylandWindow::SetBounds(const gfx::Rect& bounds_px) {
  if (bounds_px_ == bounds_px)
    return;
  bounds_px_ = bounds_px;

  // Opaque region is based on the size of the window. Thus, update the region
  // on each update.
  MaybeUpdateOpaqueRegion();

  delegate_->OnBoundsChanged(bounds_px_);
}

gfx::Rect WaylandWindow::GetBounds() {
  return bounds_px_;
}

void WaylandWindow::SetTitle(const base::string16& title) {}

void WaylandWindow::SetCapture() {
  // Wayland does implicit grabs, and doesn't allow for explicit grabs. The
  // exception to that are menus, but we explicitly send events to a
  // parent menu if such exists.
}

void WaylandWindow::ReleaseCapture() {
  // See comment in SetCapture() for details on wayland and grabs.
}

bool WaylandWindow::HasCapture() const {
  return has_implicit_grab_;
}

void WaylandWindow::ToggleFullscreen() {}

void WaylandWindow::Maximize() {}

void WaylandWindow::Minimize() {}

void WaylandWindow::Restore() {}

PlatformWindowState WaylandWindow::GetPlatformWindowState() const {
  // Remove normal state for all the other types of windows as it's only the
  // WaylandSurface that supports state changes.
  return PlatformWindowState::kNormal;
}

void WaylandWindow::Activate() {
  NOTIMPLEMENTED_LOG_ONCE();
}

void WaylandWindow::Deactivate() {
  NOTIMPLEMENTED_LOG_ONCE();
}

void WaylandWindow::SetUseNativeFrame(bool use_native_frame) {
  // See comment below in ShouldUseNativeFrame.
  NOTIMPLEMENTED_LOG_ONCE();
}

bool WaylandWindow::ShouldUseNativeFrame() const {
  // This depends on availability of XDG-Decoration protocol extension.
  NOTIMPLEMENTED_LOG_ONCE();
  return false;
}

void WaylandWindow::SetCursor(PlatformCursor cursor) {
  scoped_refptr<BitmapCursorOzone> bitmap =
      BitmapCursorFactoryOzone::GetBitmapCursor(cursor);
  if (bitmap_ == bitmap)
    return;

  bitmap_ = bitmap;

  if (bitmap_) {
    connection_->SetCursorBitmap(bitmap_->bitmaps(), bitmap_->hotspot());
  } else {
    connection_->SetCursorBitmap(std::vector<SkBitmap>(), gfx::Point());
  }
}

void WaylandWindow::MoveCursorTo(const gfx::Point& location) {
  NOTIMPLEMENTED();
}

void WaylandWindow::ConfineCursorToBounds(const gfx::Rect& bounds) {
  NOTIMPLEMENTED();
}

void WaylandWindow::SetRestoredBoundsInPixels(const gfx::Rect& bounds_px) {
  restored_bounds_px_ = bounds_px;
}

gfx::Rect WaylandWindow::GetRestoredBoundsInPixels() const {
  return restored_bounds_px_;
}

bool WaylandWindow::ShouldWindowContentsBeTransparent() const {
  NOTIMPLEMENTED_LOG_ONCE();
  return false;
}

void WaylandWindow::SetAspectRatio(const gfx::SizeF& aspect_ratio) {
  NOTIMPLEMENTED_LOG_ONCE();
}

void WaylandWindow::SetWindowIcons(const gfx::ImageSkia& window_icon,
                                   const gfx::ImageSkia& app_icon) {
  NOTIMPLEMENTED_LOG_ONCE();
}

void WaylandWindow::SizeConstraintsChanged() {}

bool WaylandWindow::CanDispatchEvent(const PlatformEvent& event) {
  // This window is a nested menu window, all the events must be forwarded
  // to the main menu window.
  if (child_window_ && wl::IsMenuType(child_window_->type()))
    return wl::IsMenuType(type());

  // If this is a nested menu window with a parent, it mustn't recieve any
  // events.
  if (parent_window_ && wl::IsMenuType(parent_window_->type()))
    return false;

  // If another window has capture, return early before checking focus.
  if (HasCapture())
    return true;

  if (event->IsMouseEvent())
    return has_pointer_focus_;
  if (event->IsKeyEvent())
    return has_keyboard_focus_;
  if (event->IsTouchEvent())
    return has_touch_focus_;
  return false;
}

uint32_t WaylandWindow::DispatchEvent(const PlatformEvent& native_event) {
  Event* event = static_cast<Event*>(native_event);
  if (event->IsLocatedEvent()) {
    // Wayland sends locations in DIP so they need to be translated to
    // physical pixels.
    event->AsLocatedEvent()->set_location_f(gfx::ScalePoint(
        event->AsLocatedEvent()->location_f(), buffer_scale_, buffer_scale_));

    // If the window does not have a pointer focus, but received this event, it
    // means the window is a menu window with a child menu window. In this case,
    // the location of the event must be converted from the nested menu to the
    // main menu, which the menu controller needs to properly handle events.
    if (wl::IsMenuType(type())) {
      // Parent window of the main menu window is not a menu, but rather an
      // xdg surface.
      DCHECK(!wl::IsMenuType(parent_window_->type()) ||
             parent_window_->type() != PlatformWindowType::kTooltip);
      auto* window =
          connection_->wayland_window_manager()->GetCurrentFocusedWindow();
      if (window) {
        ConvertEventLocationToTargetWindowLocation(GetBounds().origin(),
                                                   window->GetBounds().origin(),
                                                   event->AsLocatedEvent());
      }
    }

    auto copied_event = Event::Clone(*event);
    UpdateCursorPositionFromEvent(std::move(copied_event));
  }

  DispatchEventFromNativeUiEvent(
      native_event, base::BindOnce(&PlatformWindowDelegate::DispatchEvent,
                                   base::Unretained(delegate_)));
  return POST_DISPATCH_STOP_PROPAGATION;
}

void WaylandWindow::HandleSurfaceConfigure(int32_t widht,
                                           int32_t height,
                                           bool is_maximized,
                                           bool is_fullscreen,
                                           bool is_activated) {
  NOTREACHED()
      << "Only shell surfaces must receive HandleSurfaceConfigure calls.";
}

void WaylandWindow::HandlePopupConfigure(const gfx::Rect& bounds_dip) {
  NOTREACHED() << "Only shell popups must receive HandlePopupConfigure calls.";
}

void WaylandWindow::OnCloseRequest() {
  delegate_->OnCloseRequest();
}

void WaylandWindow::OnDragEnter(const gfx::PointF& point,
                                std::unique_ptr<OSExchangeData> data,
                                int operation) {}

int WaylandWindow::OnDragMotion(const gfx::PointF& point,
                                uint32_t time,
                                int operation) {
  return -1;
}

void WaylandWindow::OnDragDrop(std::unique_ptr<OSExchangeData> data) {}

void WaylandWindow::OnDragLeave() {}

void WaylandWindow::OnDragSessionClose(uint32_t dnd_action) {}

void WaylandWindow::SetBoundsDip(const gfx::Rect& bounds_dip) {
  SetBounds(gfx::ScaleToRoundedRect(bounds_dip, buffer_scale_));
}

bool WaylandWindow::Initialize(PlatformWindowInitProperties properties) {
  // Properties contain DIP bounds but the buffer scale is initially 1 so it's
  // OK to assign.  The bounds will be recalculated when the buffer scale
  // changes.
  DCHECK_EQ(buffer_scale_, 1);
  bounds_px_ = properties.bounds;
  opacity_ = properties.opacity;
  type_ = properties.type;

  surface_.reset(wl_compositor_create_surface(connection_->compositor()));
  if (!surface_) {
    LOG(ERROR) << "Failed to create wl_surface";
    return false;
  }
  wl_surface_set_user_data(surface_.get(), this);
  AddSurfaceListener();

  connection_->wayland_window_manager()->AddWindow(GetWidget(), this);

  if (!OnInitialize(std::move(properties)))
    return false;

  connection_->ScheduleFlush();

  PlatformEventSource::GetInstance()->AddPlatformEventDispatcher(this);
  delegate_->OnAcceleratedWidgetAvailable(GetWidget());

  // Will do nothing for menus because they have got their scale above.
  UpdateBufferScale(false);

  MaybeUpdateOpaqueRegion();
  return true;
}

void WaylandWindow::SetBufferScale(int32_t new_scale, bool update_bounds) {
  DCHECK_GT(new_scale, 0);

  if (new_scale == buffer_scale_)
    return;

  auto old_scale = buffer_scale_;
  buffer_scale_ = new_scale;
  if (update_bounds)
    SetBoundsDip(gfx::ScaleToRoundedRect(bounds_px_, 1.0 / old_scale));

  DCHECK(surface());
  wl_surface_set_buffer_scale(surface(), buffer_scale_);
  connection_->ScheduleFlush();
}

WaylandWindow* WaylandWindow::GetParentWindow(
    gfx::AcceleratedWidget parent_widget) {
  auto* parent_window =
      connection_->wayland_window_manager()->GetWindow(parent_widget);

  // If propagated parent has already had a child, it means that |this| is a
  // submenu of a 3-dot menu. In aura, the parent of a 3-dot menu and its
  // submenu is the main native widget, which is the main window. In contrast,
  // Wayland requires a menu window to be a parent of a submenu window. Thus,
  // check if the suggested parent has a child. If yes, take its child as a
  // parent of |this|.
  // Another case is a notifcation window or a drop down window, which do not
  // have a parent in aura. In this case, take the current focused window as a
  // parent.
  if (parent_window && parent_window->child_window_)
    return parent_window->child_window_;
  if (!parent_window)
    return connection_->wayland_window_manager()->GetCurrentFocusedWindow();
  return parent_window;
}

WaylandWindow* WaylandWindow::GetRootParentWindow() {
  return parent_window_ ? parent_window_->GetRootParentWindow() : this;
}

void WaylandWindow::AddSurfaceListener() {
  static struct wl_surface_listener surface_listener = {
      &WaylandWindow::Enter,
      &WaylandWindow::Leave,
  };
  wl_surface_add_listener(surface_.get(), &surface_listener, this);
}

void WaylandWindow::AddEnteredOutputId(struct wl_output* output) {
  // Wayland does weird things for menus so instead of tracking outputs that
  // we entered or left, we take that from the parent window and ignore this
  // event.
  if (wl::IsMenuType(type()))
    return;

  const uint32_t entered_output_id =
      connection_->wayland_output_manager()->GetIdForOutput(output);
  DCHECK_NE(entered_output_id, 0u);
  auto result = entered_outputs_ids_.insert(entered_output_id);
  DCHECK(result.first != entered_outputs_ids_.end());

  UpdateBufferScale(true);
}

void WaylandWindow::RemoveEnteredOutputId(struct wl_output* output) {
  // Wayland does weird things for menus so instead of tracking outputs that
  // we entered or left, we take that from the parent window and ignore this
  // event.
  if (wl::IsMenuType(type()))
    return;

  const uint32_t left_output_id =
      connection_->wayland_output_manager()->GetIdForOutput(output);
  auto entered_output_id_it = entered_outputs_ids_.find(left_output_id);
  // Workaround: when a user switches physical output between two displays,
  // a window does not necessarily receive enter events immediately or until
  // a user resizes/moves the window. It means that switching output between
  // displays in a single output mode results in leave events, but the surface
  // might not have received enter event before. Thus, remove the id of left
  // output only if it was stored before.
  if (entered_output_id_it != entered_outputs_ids_.end())
    entered_outputs_ids_.erase(entered_output_id_it);

  UpdateBufferScale(true);
}

void WaylandWindow::UpdateCursorPositionFromEvent(
    std::unique_ptr<Event> event) {
  DCHECK(event->IsLocatedEvent());

  // This is a tricky part. Initially, Wayland sends events to surfaces the
  // events are targeted for. But, in order to fulfill Chromium's assumptions
  // about event targets, some of the events are rerouted and their locations
  // are converted. The event we got here is rerouted and it has had its
  // location fixed.
  //
  // Basically, this method must translate coordinates of all events
  // in regards to top-level windows' coordinates as it's always located at
  // origin (0,0) from Chromium point of view (remember that Wayland doesn't
  // provide global coordinates to its clients). And it's totally fine to use it
  // as the target. Thus, the location of the |event| is always converted using
  // the top-level window's bounds as the target excluding cases, when the
  // mouse/touch is over a top-level window.
  auto* toplevel_window = GetRootParentWindow();
  if (toplevel_window != this) {
    ConvertEventLocationToTargetWindowLocation(
        toplevel_window->GetBounds().origin(), GetBounds().origin(),
        event->AsLocatedEvent());
  }
  auto* cursor_position = connection_->wayland_cursor_position();
  if (cursor_position) {
    cursor_position->OnCursorPositionChanged(
        event->AsLocatedEvent()->location());
  }
}

WaylandWindow* WaylandWindow::GetTopLevelWindow() {
  return parent_window_ ? parent_window_->GetTopLevelWindow() : this;
}

void WaylandWindow::MaybeUpdateOpaqueRegion() {
  if (!IsOpaqueWindow())
    return;

  wl::Object<wl_region> region(
      wl_compositor_create_region(connection_->compositor()));
  wl_region_add(region.get(), 0, 0, bounds_px_.width(), bounds_px_.height());
  wl_surface_set_opaque_region(surface(), region.get());

  connection_->ScheduleFlush();
}

bool WaylandWindow::IsOpaqueWindow() const {
  return opacity_ == ui::PlatformWindowOpacity::kOpaqueWindow;
}

// static
void WaylandWindow::Enter(void* data,
                          struct wl_surface* wl_surface,
                          struct wl_output* output) {
  auto* window = static_cast<WaylandWindow*>(data);
  if (window) {
    DCHECK(window->surface_.get() == wl_surface);
    window->AddEnteredOutputId(output);
  }
}

// static
void WaylandWindow::Leave(void* data,
                          struct wl_surface* wl_surface,
                          struct wl_output* output) {
  auto* window = static_cast<WaylandWindow*>(data);
  if (window) {
    DCHECK(window->surface_.get() == wl_surface);
    window->RemoveEnteredOutputId(output);
  }
}

}  // namespace ui
