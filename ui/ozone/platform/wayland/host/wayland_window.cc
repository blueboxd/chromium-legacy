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
#include "ui/gfx/geometry/point_f.h"
#include "ui/ozone/platform/wayland/common/wayland_util.h"
#include "ui/ozone/platform/wayland/host/shell_object_factory.h"
#include "ui/ozone/platform/wayland/host/shell_popup_wrapper.h"
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

  if (has_pointer_focus_)
    connection_->pointer()->reset_window_with_pointer_focus();
}

// static
WaylandWindow* WaylandWindow::FromSurface(wl_surface* surface) {
  return static_cast<WaylandWindow*>(
      wl_proxy_get_user_data(reinterpret_cast<wl_proxy*>(surface)));
}

void WaylandWindow::UpdateBufferScale(bool update_bounds) {
  DCHECK(connection_->wayland_output_manager());
  const auto* screen = connection_->wayland_output_manager()->wayland_screen();
  DCHECK(screen);
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

void WaylandWindow::CreateShellPopup() {
  if (bounds_px_.IsEmpty())
    return;

  // TODO(jkim): Consider how to support DropArrow window on tabstrip.
  // When it starts dragging, as described the protocol, https://goo.gl/1Mskq3,
  // the client must have an active implicit grab. If we try to create a popup
  // window while dragging is executed, it gets 'popup_done' directly from
  // Wayland compositor and it's destroyed through 'popup_done'. It causes
  // a crash when aura::Window is destroyed.
  // https://crbug.com/875164
  if (connection_->IsDragInProgress()) {
    surface_.reset();
    LOG(ERROR) << "Wayland can't create a popup window during dragging.";
    return;
  }

  DCHECK(parent_window_ && !shell_popup_);

  auto bounds_px = AdjustPopupWindowPosition();

  ShellObjectFactory factory;
  shell_popup_ = factory.CreateShellPopupWrapper(connection_, this, bounds_px);
  if (!shell_popup_)
    CHECK(false) << "Failed to create Wayland shell popup";

  parent_window_->set_child_window(this);
}

void WaylandWindow::CreateAndShowTooltipSubSurface() {
  // Since Aura does not not provide a reference parent window, needed by
  // Wayland, we get the current focused window to place and show the tooltips.
  auto* parent_window =
      connection_->wayland_window_manager()->GetCurrentFocusedWindow();

  // Tooltip creation is an async operation. By the time Aura actually creates
  // the tooltip, it is possible that the user has already moved the
  // mouse/pointer out of the window that triggered the tooptip. In this case,
  // parent_window is NULL.
  if (!parent_window)
    return;

  wl_subcompositor* subcompositor = connection_->subcompositor();
  DCHECK(subcompositor);
  tooltip_subsurface_.reset(wl_subcompositor_get_subsurface(
      subcompositor, surface_.get(), parent_window->surface()));

  // Chromium positions tooltip windows in screen coordinates, but Wayland
  // requires them to be in local surface coordinates aka relative to parent
  // window.
  const auto parent_bounds_dip =
      gfx::ScaleToRoundedRect(parent_window->GetBounds(), 1.0 / ui_scale_);
  auto new_bounds_dip =
      wl::TranslateBoundsToParentCoordinates(bounds_px_, parent_bounds_dip);
  auto bounds_px =
      gfx::ScaleToRoundedRect(new_bounds_dip, ui_scale_ / buffer_scale_);

  DCHECK(tooltip_subsurface_);
  // Convert position to DIP.
  wl_subsurface_set_position(tooltip_subsurface_.get(),
                             bounds_px.x() / buffer_scale_,
                             bounds_px.y() / buffer_scale_);
  wl_subsurface_set_desync(tooltip_subsurface_.get());
  wl_surface_commit(parent_window->surface());
  connection_->ScheduleFlush();
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
  if (!is_tooltip_)  // Tooltip windows should not get keyboard focus
    set_keyboard_focus(true);

  if (is_tooltip_) {
    CreateAndShowTooltipSubSurface();
    return;
  }

  if (!shell_popup_) {
    // When showing a sub-menu after it has been previously shown and hidden,
    // Wayland sends SetBounds prior to Show, and |bounds_px| takes the pixel
    // bounds.  This makes a difference against the normal flow when the
    // window is created (see |Initialize|).  To equalize things, rescale
    // |bounds_px_| to DIP.  It will be adjusted while creating the popup.
    bounds_px_ = gfx::ScaleToRoundedRect(bounds_px_, 1.0 / ui_scale_);
    CreateShellPopup();
    connection_->ScheduleFlush();
  }

  UpdateBufferScale(false);
}

void WaylandWindow::Hide() {
  if (is_tooltip_) {
    tooltip_subsurface_.reset();
  } else {
    if (child_window_)
      child_window_->Hide();
    if (shell_popup_) {
      parent_window_->set_child_window(nullptr);
      shell_popup_.reset();
    }
  }

  // Detach buffer from surface in order to completely shutdown popups and
  // tooltips, and release resources.
  connection_->buffer_manager_host()->ResetSurfaceContents(GetWidget());
}

void WaylandWindow::Close() {
  delegate_->OnClosed();
}

bool WaylandWindow::IsVisible() const {
  return !!shell_popup_;
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
  // exception to that are popups, but we explicitly send events to a
  // parent popup if such exists.
}

void WaylandWindow::ReleaseCapture() {
  // See comment in SetCapture() for details on wayland and grabs.
}

bool WaylandWindow::HasCapture() const {
  // If WaylandWindow is a popup window, assume it has the capture.
  return shell_popup() ? true : has_implicit_grab_;
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
  // This window is a nested popup window, all the events must be forwarded
  // to the main popup window.
  if (child_window_ && child_window_->shell_popup())
    return !!shell_popup_.get();

  // If this is a nested menu window with a parent, it mustn't recieve any
  // events.
  if (parent_window_ && parent_window_->shell_popup())
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
    auto copied_event = Event::Clone(*event);
    UpdateCursorPositionFromEvent(std::move(copied_event));
  }

  // If the window does not have a pointer focus, but received this event, it
  // means the window is a popup window with a child popup window. In this case,
  // the location of the event must be converted from the nested popup to the
  // main popup, which the menu controller needs to properly handle events.
  if (event->IsLocatedEvent() && shell_popup()) {
    // Parent window of the main menu window is not a popup, but rather an
    // xdg surface.
    DCHECK(!parent_window_->shell_popup() || !parent_window_->is_tooltip_);
    auto* window =
        connection_->wayland_window_manager()->GetCurrentFocusedWindow();
    if (window) {
      ConvertEventLocationToTargetWindowLocation(GetBounds().origin(),
                                                 window->GetBounds().origin(),
                                                 event->AsLocatedEvent());
    }
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
  DCHECK(shell_popup());
  DCHECK(parent_window_);

  SetBufferScale(parent_window_->buffer_scale_, true);

  gfx::Rect new_bounds_dip = bounds_dip;

  // It's not enough to just set new bounds. If it is a menu window, whose
  // parent is a top level window aka browser window, it can be flipped
  // vertically along y-axis and have negative values set. Chromium cannot
  // understand that and starts to position nested menu windows incorrectly. To
  // fix that, we have to bear in mind that Wayland compositor does not share
  // global coordinates for any surfaces, and Chromium assumes the top level
  // window is always located at 0,0 origin. What is more, child windows must
  // always be positioned relative to parent window local surface coordinates.
  // Thus, if the menu window is flipped along y-axis by Wayland and its origin
  // is above the top level parent window, the origin of the top level window
  // has to be shifted by that value on y-axis so that the origin of the menu
  // becomes x,0, and events can be handled normally.
  if (!parent_window_->shell_popup()) {
    gfx::Rect parent_bounds = parent_window_->GetBounds();
    // The menu window is flipped along y-axis and have x,-y origin. Shift the
    // parent top level window instead.
    if (new_bounds_dip.y() < 0) {
      // Move parent bounds along y-axis.
      parent_bounds.set_y(-(new_bounds_dip.y() * buffer_scale_));
      new_bounds_dip.set_y(0);
    } else {
      // If the menu window is located at correct origin from the browser point
      // of view, return the top level window back to 0,0.
      parent_bounds.set_y(0);
    }
    parent_window_->SetBounds(parent_bounds);
  } else {
    // The nested menu windows are located relative to the parent menu windows.
    // Thus, the location must be translated to be relative to the top level
    // window, which automatically becomes the same as relative to an origin of
    // a display.
    new_bounds_dip = gfx::ScaleToRoundedRect(
        wl::TranslateBoundsToTopLevelCoordinates(
            gfx::ScaleToRoundedRect(new_bounds_dip, buffer_scale_),
            parent_window_->GetBounds()),
        1.0 / buffer_scale_);
    DCHECK(new_bounds_dip.y() >= 0);
  }

  SetBoundsDip(new_bounds_dip);
}

void WaylandWindow::OnCloseRequest() {
  // Before calling OnCloseRequest, the |shell_popup_| must become hidden and
  // only then call OnCloseRequest().
  DCHECK(!shell_popup_);
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

  surface_.reset(wl_compositor_create_surface(connection_->compositor()));
  if (!surface_) {
    LOG(ERROR) << "Failed to create wl_surface";
    return false;
  }
  wl_surface_set_user_data(surface_.get(), this);
  AddSurfaceListener();

  connection_->wayland_window_manager()->AddWindow(GetWidget(), this);

  ui::PlatformWindowType ui_window_type = properties.type;
  switch (ui_window_type) {
    case ui::PlatformWindowType::kMenu:
    case ui::PlatformWindowType::kPopup:
      parent_window_ = GetParentWindow(properties.parent_widget);

      // Popups need to know their scale earlier to position themselves.
      if (!parent_window_) {
        LOG(ERROR) << "Failed to get a parent window for this popup";
        return false;
      }

      SetBufferScale(parent_window_->buffer_scale_, false);
      ui_scale_ = parent_window_->ui_scale_;

      // TODO(msisov, jkim): Handle notification windows, which are marked
      // as popup windows as well. Those are the windows that do not have
      // parents and pop up when the browser receives a notification.
      CreateShellPopup();
      break;
    case ui::PlatformWindowType::kTooltip:
      // Tooltips subsurfaces are created on demand, upon ::Show calls.
      is_tooltip_ = true;
      break;
    case ui::PlatformWindowType::kWindow:
    case ui::PlatformWindowType::kBubble:
    case ui::PlatformWindowType::kDrag:
      if (!OnInitialize(std::move(properties)))
        return false;
      break;
  }

  connection_->ScheduleFlush();

  PlatformEventSource::GetInstance()->AddPlatformEventDispatcher(this);
  delegate_->OnAcceleratedWidgetAvailable(GetWidget());

  // Will do nothing for popups because they have got their scale above.
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
  // Wayland does weird things for popups so instead of tracking outputs that
  // we entered or left, we take that from the parent window and ignore this
  // event.
  if (shell_popup())
    return;

  const uint32_t entered_output_id =
      connection_->wayland_output_manager()->GetIdForOutput(output);
  DCHECK_NE(entered_output_id, 0u);
  auto result = entered_outputs_ids_.insert(entered_output_id);
  DCHECK(result.first != entered_outputs_ids_.end());

  UpdateBufferScale(true);
}

void WaylandWindow::RemoveEnteredOutputId(struct wl_output* output) {
  // Wayland does weird things for popups so instead of tracking outputs that
  // we entered or left, we take that from the parent window and ignore this
  // event.
  if (shell_popup())
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
  auto* window =
      connection_->wayland_window_manager()->GetCurrentFocusedWindow();
  // This is a tricky part. Initially, Wayland sends events to surfaces the
  // events are targeted for. But, in order to fulfill Chromium's assumptions
  // about event targets, some of the events are rerouted and their locations
  // are converted.
  //
  // The event we got here is rerouted, but it hasn't had its location fixed
  // yet. Passing an event with fixed location won't help as well - its location
  // is converted in a different way: if mouse is moved outside a menu window
  // to the left, the location of such event includes negative values.
  //
  // In contrast, this method must translate coordinates of all events
  // in regards to top-level windows' coordinates as it's always located at
  // origin (0,0) from Chromium point of view (remember that Wayland doesn't
  // provide global coordinates to its clients). And it's totally fine to use it
  // as the target. Thus, the location of the |event| is always converted using
  // the top-level window's bounds as the target excluding cases, when the
  // mouse/touch is over a top-level window.
  if (parent_window_ && parent_window_ != window) {
    const gfx::Rect target_bounds = parent_window_->GetBounds();
    gfx::Rect own_bounds = GetBounds();
    // This is a bit trickier, and concerns nested menu windows. Whenever an
    // event is sent to the nested menu window, it's rerouted to a parent menu
    // window. Thus, in order to correctly translate its location, we must
    // choose correct values for the |own_bounds|. In this case, it must the
    // nested menu window, because |this| is the parent of that window.
    if (window == child_window_)
      own_bounds = child_window_->GetBounds();
    ConvertEventLocationToTargetWindowLocation(
        target_bounds.origin(), own_bounds.origin(), event->AsLocatedEvent());
  }
  auto* cursor_position = connection_->wayland_cursor_position();
  if (cursor_position) {
    cursor_position->OnCursorPositionChanged(
        event->AsLocatedEvent()->location());
  }
}

gfx::Rect WaylandWindow::AdjustPopupWindowPosition() const {
  auto* parent_window = parent_window_->shell_popup()
                            ? parent_window_->parent_window_
                            : parent_window_;
  DCHECK(parent_window);
  DCHECK(buffer_scale_ == parent_window->buffer_scale_);
  DCHECK(ui_scale_ == parent_window->ui_scale_);

  // Chromium positions windows in screen coordinates, but Wayland requires them
  // to be in local surface coordinates aka relative to parent window.
  const gfx::Rect parent_bounds_dip =
      gfx::ScaleToRoundedRect(parent_window_->GetBounds(), 1.0 / ui_scale_);
  gfx::Rect new_bounds_dip =
      wl::TranslateBoundsToParentCoordinates(bounds_px_, parent_bounds_dip);

  // Chromium may decide to position nested menu windows on the left side
  // instead of the right side of parent menu windows when the size of the
  // window becomes larger than the display it is shown on. It's correct when
  // the window is located on one display and occupies the whole work area, but
  // as soon as it's moved and there is space on the right side, Chromium
  // continues positioning the nested menus on the left side relative to the
  // parent menu (Wayland does not provide clients with global coordinates).
  // Instead, reposition that window to be on the right side of the parent menu
  // window and let the compositor decide how to position it if it does not fit
  // a single display. However, there is one exception - if the window is
  // maximized, let Chromium position it on the left side as long as the Wayland
  // compositor may decide to position the nested window on the right side of
  // the parent menu window, which results in showing it on a second display if
  // more than one display is used.
  if (parent_window_->shell_popup() && parent_window_->parent_window_ &&
      (parent_window_->parent_window()->GetPlatformWindowState() !=
       PlatformWindowState::kMaximized)) {
    auto* top_level_window = parent_window_->parent_window_;
    DCHECK(top_level_window && !top_level_window->shell_popup());
    if (new_bounds_dip.x() <= 0 && top_level_window->GetPlatformWindowState() !=
                                       PlatformWindowState::kMaximized) {
      // Position the child menu window on the right side of the parent window
      // and let the Wayland compositor decide how to do constraint
      // adjustements.
      int new_x = parent_bounds_dip.width() -
                  (new_bounds_dip.width() + new_bounds_dip.x());
      new_bounds_dip.set_x(new_x);
    }
  }
  return gfx::ScaleToRoundedRect(new_bounds_dip, ui_scale_ / buffer_scale_);
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

bool WaylandWindow::OnInitialize(PlatformWindowInitProperties properties) {
  return true;
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
