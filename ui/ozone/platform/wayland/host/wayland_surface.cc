// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/ozone/platform/wayland/host/wayland_surface.h"

#include "ui/base/dragdrop/drag_drop_types.h"
#include "ui/base/dragdrop/os_exchange_data.h"
#include "ui/base/hit_test.h"
#include "ui/gfx/native_widget_types.h"
#include "ui/ozone/platform/wayland/host/shell_object_factory.h"
#include "ui/ozone/platform/wayland/host/shell_surface_wrapper.h"
#include "ui/ozone/platform/wayland/host/wayland_buffer_manager_host.h"
#include "ui/ozone/platform/wayland/host/wayland_connection.h"
#include "ui/platform_window/platform_window_handler/wm_drop_handler.h"

namespace ui {

WaylandSurface::WaylandSurface(PlatformWindowDelegate* delegate,
                               WaylandConnection* connection)
    : WaylandWindow(delegate, connection),
      state_(PlatformWindowState::kNormal) {
  // Set a class property key, which allows |this| to be used for interactive
  // events, e.g. move or resize.
  SetWmMoveResizeHandler(this, AsWmMoveResizeHandler());

  // Set a class property key, which allows |this| to be used for drag action.
  SetWmDragHandler(this, this);
}

WaylandSurface::~WaylandSurface() {
  if (drag_closed_callback_) {
    std::move(drag_closed_callback_)
        .Run(DragDropTypes::DragOperation::DRAG_NONE);
  }
}

bool WaylandSurface::CreateShellSurface() {
  ShellObjectFactory factory;
  shell_surface_ = factory.CreateShellSurfaceWrapper(connection(), this);
  if (!shell_surface_) {
    LOG(ERROR) << "Failed to create a ShellSurface.";
    return false;
  }

  shell_surface_->SetAppId(app_id_);
  shell_surface_->SetTitle(window_title_);
  SetSizeConstraints();
  TriggerStateChanges();
  return true;
}

void WaylandSurface::ApplyPendingBounds() {
  if (pending_bounds_dip_.IsEmpty())
    return;
  DCHECK(shell_surface_);

  SetBoundsDip(pending_bounds_dip_);
  shell_surface_->SetWindowGeometry(pending_bounds_dip_);
  pending_bounds_dip_ = gfx::Rect();
  connection()->ScheduleFlush();
}

void WaylandSurface::DispatchHostWindowDragMovement(
    int hittest,
    const gfx::Point& pointer_location_in_px) {
  DCHECK(shell_surface_);

  connection()->ResetPointerFlags();
  if (hittest == HTCAPTION)
    shell_surface_->SurfaceMove(connection());
  else
    shell_surface_->SurfaceResize(connection(), hittest);

  connection()->ScheduleFlush();
}

void WaylandSurface::StartDrag(const ui::OSExchangeData& data,
                               int operation,
                               gfx::NativeCursor cursor,
                               base::OnceCallback<void(int)> callback) {
  DCHECK(!drag_closed_callback_);
  drag_closed_callback_ = std::move(callback);
  connection()->StartDrag(data, operation);
}

void WaylandSurface::Show(bool inactive) {
  if (shell_surface_)
    return;

  if (!CreateShellSurface()) {
    Close();
    return;
  }

  set_keyboard_focus(true);
  UpdateBufferScale(false);
}

void WaylandSurface::Hide() {
  if (!shell_surface_)
    return;

  if (child_window()) {
    child_window()->Hide();
    set_child_window(nullptr);
  }

  shell_surface_.reset();
  connection()->ScheduleFlush();

  // Detach buffer from surface in order to completely shutdown menus and
  // tooltips, and release resources.
  connection()->buffer_manager_host()->ResetSurfaceContents(GetWidget());
}

bool WaylandSurface::IsVisible() const {
  // X and Windows return true if the window is minimized. For consistency, do
  // the same.
  return !!shell_surface_ || state_ == PlatformWindowState::kMinimized;
}

void WaylandSurface::SetTitle(const base::string16& title) {
  if (window_title_ == title)
    return;

  window_title_ = title;

  if (shell_surface_) {
    shell_surface_->SetTitle(title);
    connection()->ScheduleFlush();
  }
}

void WaylandSurface::ToggleFullscreen() {
  // TODO(msisov, tonikitoo): add multiscreen support. As the documentation says
  // if xdg_toplevel_set_fullscreen() is not provided with wl_output, it's up
  // to the compositor to choose which display will be used to map this surface.

  // We must track the previous state to correctly say our state as long as it
  // can be the maximized instead of normal one.
  PlatformWindowState new_state = PlatformWindowState::kUnknown;
  if (state_ == PlatformWindowState::kFullScreen) {
    if (previous_state_ == PlatformWindowState::kMaximized)
      new_state = previous_state_;
    else
      new_state = PlatformWindowState::kNormal;
  } else {
    new_state = PlatformWindowState::kFullScreen;
  }

  SetWindowState(new_state);
}

void WaylandSurface::Maximize() {
  SetWindowState(PlatformWindowState::kMaximized);
}

void WaylandSurface::Minimize() {
  SetWindowState(PlatformWindowState::kMinimized);
}

void WaylandSurface::Restore() {
  DCHECK(shell_surface_);
  SetWindowState(PlatformWindowState::kNormal);
}

PlatformWindowState WaylandSurface::GetPlatformWindowState() const {
  return state_;
}

void WaylandSurface::SizeConstraintsChanged() {
  // Size constraints only make sense for normal windows.
  if (!shell_surface_)
    return;

  DCHECK(delegate());
  min_size_ = delegate()->GetMinimumSizeForWindow();
  max_size_ = delegate()->GetMaximumSizeForWindow();
  SetSizeConstraints();
}

void WaylandSurface::HandleSurfaceConfigure(int32_t width,
                                            int32_t height,
                                            bool is_maximized,
                                            bool is_fullscreen,
                                            bool is_activated) {
  // Store the old state to propagte state changes if Wayland decides to change
  // the state to something else.
  PlatformWindowState old_state = state_;
  if (state_ == PlatformWindowState::kMinimized && !is_activated) {
    state_ = PlatformWindowState::kMinimized;
  } else if (is_fullscreen) {
    state_ = PlatformWindowState::kFullScreen;
  } else if (is_maximized) {
    state_ = PlatformWindowState::kMaximized;
  } else {
    state_ = PlatformWindowState::kNormal;
  }

  const bool state_changed = old_state != state_;
  const bool is_normal = state_ == PlatformWindowState::kNormal;

  // Update state before notifying delegate.
  const bool did_active_change = is_active_ != is_activated;
  is_active_ = is_activated;

  // Rather than call SetBounds here for every configure event, just save the
  // most recent bounds, and have WaylandConnection call ApplyPendingBounds
  // when it has finished processing events. We may get many configure events
  // in a row during an interactive resize, and only the last one matters.
  //
  // Width or height set to 0 means that we should decide on width and height by
  // ourselves, but we don't want to set them to anything else. Use restored
  // bounds size or the current bounds iff the current state is normal (neither
  // maximized nor fullscreen).
  //
  // Note: if the browser was started with --start-fullscreen and a user exits
  // the fullscreen mode, wayland may set the width and height to be 1. Instead,
  // explicitly set the bounds to the current desired ones or the previous
  // bounds.
  if (width > 1 && height > 1) {
    pending_bounds_dip_ = gfx::Rect(0, 0, width, height);
  } else if (is_normal) {
    pending_bounds_dip_.set_size(
        gfx::ScaleToRoundedSize(GetRestoredBoundsInPixels().IsEmpty()
                                    ? GetBounds().size()
                                    : GetRestoredBoundsInPixels().size(),

                                1.0 / buffer_scale()));
  }

  // Store the restored bounds of current state differs from the normal state.
  // It can be client or compositor side change from normal to something else.
  // Thus, we must store previous bounds to restore later.
  SetOrResetRestoredBounds();
  ApplyPendingBounds();

  if (state_changed)
    delegate()->OnWindowStateChanged(state_);

  if (did_active_change)
    delegate()->OnActivationChanged(is_active_);
}

void WaylandSurface::OnDragEnter(const gfx::PointF& point,
                                 std::unique_ptr<OSExchangeData> data,
                                 int operation) {
  WmDropHandler* drop_handler = GetWmDropHandler(*this);
  if (!drop_handler)
    return;

  // Wayland sends locations in DIP so they need to be translated to
  // physical pixels.
  drop_handler->OnDragEnter(
      gfx::ScalePoint(point, buffer_scale(), buffer_scale()), std::move(data),
      operation);
}

int WaylandSurface::OnDragMotion(const gfx::PointF& point,
                                 uint32_t time,
                                 int operation) {
  WmDropHandler* drop_handler = GetWmDropHandler(*this);
  if (!drop_handler)
    return 0;

  // Wayland sends locations in DIP so they need to be translated to
  // physical pixels.
  return drop_handler->OnDragMotion(
      gfx::ScalePoint(point, buffer_scale(), buffer_scale()), operation);
}

void WaylandSurface::OnDragDrop(std::unique_ptr<OSExchangeData> data) {
  WmDropHandler* drop_handler = GetWmDropHandler(*this);
  if (!drop_handler)
    return;
  drop_handler->OnDragDrop(std::move(data));
}

void WaylandSurface::OnDragLeave() {
  WmDropHandler* drop_handler = GetWmDropHandler(*this);
  if (!drop_handler)
    return;
  drop_handler->OnDragLeave();
}

void WaylandSurface::OnDragSessionClose(uint32_t dnd_action) {
  std::move(drag_closed_callback_).Run(dnd_action);
  connection()->ResetPointerFlags();
}

bool WaylandSurface::OnInitialize(PlatformWindowInitProperties properties) {
  app_id_ = properties.wm_class_class;
  return true;
}

void WaylandSurface::TriggerStateChanges() {
  if (!shell_surface_)
    return;

  if (state_ == PlatformWindowState::kFullScreen)
    shell_surface_->SetFullscreen();
  else
    shell_surface_->UnSetFullscreen();

  // Call UnSetMaximized only if current state is normal. Otherwise, if the
  // current state is fullscreen and the previous is maximized, calling
  // UnSetMaximized may result in wrong restored window position that clients
  // are not allowed to know about.
  if (state_ == PlatformWindowState::kMaximized)
    shell_surface_->SetMaximized();
  else if (state_ == PlatformWindowState::kNormal)
    shell_surface_->UnSetMaximized();

  if (state_ == PlatformWindowState::kMinimized)
    shell_surface_->SetMinimized();

  connection()->ScheduleFlush();
}

void WaylandSurface::SetWindowState(PlatformWindowState state) {
  previous_state_ = state_;
  state_ = state;
  TriggerStateChanges();
}

WmMoveResizeHandler* WaylandSurface::AsWmMoveResizeHandler() {
  return static_cast<WmMoveResizeHandler*>(this);
}

void WaylandSurface::SetSizeConstraints() {
  if (min_size_.has_value())
    shell_surface_->SetMinSize(min_size_->width(), min_size_->height());
  if (max_size_.has_value())
    shell_surface_->SetMaxSize(max_size_->width(), max_size_->height());

  connection()->ScheduleFlush();
}

void WaylandSurface::SetOrResetRestoredBounds() {
  // The |restored_bounds_| are used when the window gets back to normal
  // state after it went maximized or fullscreen.  So we reset these if the
  // window has just become normal and store the current bounds if it is
  // either going out of normal state or simply changes the state and we don't
  // have any meaningful value stored.
  if (GetPlatformWindowState() == PlatformWindowState::kNormal) {
    SetRestoredBoundsInPixels({});
  } else if (GetRestoredBoundsInPixels().IsEmpty()) {
    SetRestoredBoundsInPixels(GetBounds());
  }
}

}  // namespace ui
