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
#include "ui/ozone/platform/wayland/host/wayland_connection.h"
#include "ui/platform_window/platform_window_handler/wm_drop_handler.h"

namespace ui {

WaylandSurface::WaylandSurface(PlatformWindowDelegate* delegate,
                               WaylandConnection* connection)
    : WaylandWindow(delegate, connection),
      state_(PlatformWindowState::kNormal),
      pending_state_(PlatformWindowState::kUnknown) {
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

void WaylandSurface::CreateShellSurface() {
  ShellObjectFactory factory;
  shell_surface_ = factory.CreateShellSurfaceWrapper(connection(), this);
  if (!shell_surface_)
    CHECK(false) << "Failed to initialize Wayland shell surface";
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
  set_keyboard_focus(true);
  // TODO(msisov): recreate |shell_surface_| on show calls.
}

void WaylandSurface::Hide() {
  // TODO(msisov): destroy |shell_surface_| on hide calls.
}

bool WaylandSurface::IsVisible() const {
  // X and Windows return true if the window is minimized. For consistency, do
  // the same.
  return !!shell_surface_ || IsMinimized();
}

void WaylandSurface::SetTitle(const base::string16& title) {
  DCHECK(shell_surface_);
  shell_surface_->SetTitle(title);
  connection()->ScheduleFlush();
}

void WaylandSurface::ToggleFullscreen() {
  DCHECK(shell_surface_);

  // There are some cases, when Chromium triggers a fullscreen state change
  // before the surface is activated. In such cases, Wayland may ignore state
  // changes and such flags as --kiosk or --start-fullscreen will be ignored.
  // To overcome this, set a pending state, and once the surface is activated,
  // trigger the change.
  if (!is_active_) {
    DCHECK(!IsFullscreen());
    pending_state_ = PlatformWindowState::kFullScreen;
    return;
  }

  // TODO(msisov, tonikitoo): add multiscreen support. As the documentation says
  // if shell_surface_set_fullscreen() is not provided with wl_output, it's up
  // to the compositor to choose which display will be used to map this surface.
  if (!IsFullscreen()) {
    // Fullscreen state changes have to be handled manually and then checked
    // against configuration events, which come from a compositor. The reason
    // of manually changing the |state_| is that the compositor answers about
    // state changes asynchronously, which leads to a wrong return value in
    // DesktopWindowTreeHostPlatform::IsFullscreen, for example, and media
    // files can never be set to fullscreen.
    state_ = PlatformWindowState::kFullScreen;
    shell_surface_->SetFullscreen();
  } else {
    // Check the comment above. If it's not handled synchronously, media files
    // may not leave the fullscreen mode.
    state_ = PlatformWindowState::kUnknown;
    shell_surface_->UnSetFullscreen();
  }

  connection()->ScheduleFlush();
}

void WaylandSurface::Maximize() {
  DCHECK(shell_surface_);

  if (IsFullscreen())
    ToggleFullscreen();

  shell_surface_->SetMaximized();
  connection()->ScheduleFlush();
}

void WaylandSurface::Minimize() {
  DCHECK(shell_surface_);
  DCHECK(!is_minimizing_);
  // Wayland doesn't explicitly say if a window is minimized. Instead, it
  // notifies that the window is not activated. But there are many cases, when
  // the window is not minimized and deactivated. In order to properly record
  // the minimized state, mark this window as being minimized. And as soon as a
  // configuration event comes, check if the window has been deactivated and has
  // |is_minimizing_| set.
  is_minimizing_ = true;
  shell_surface_->SetMinimized();
  connection()->ScheduleFlush();
}

void WaylandSurface::Restore() {
  DCHECK(shell_surface_);

  // Unfullscreen the window if it is fullscreen.
  if (IsFullscreen())
    ToggleFullscreen();

  shell_surface_->UnSetMaximized();
  connection()->ScheduleFlush();
}

PlatformWindowState WaylandSurface::GetPlatformWindowState() const {
  return state_;
}

void WaylandSurface::SizeConstraintsChanged() {
  // Size constraints only make sense for normal windows.
  if (!shell_surface_)
    return;

  DCHECK(delegate());
  auto min_size = delegate()->GetMinimumSizeForWindow();
  auto max_size = delegate()->GetMaximumSizeForWindow();

  if (min_size.has_value())
    shell_surface_->SetMinSize(min_size->width(), min_size->height());
  if (max_size.has_value())
    shell_surface_->SetMaxSize(max_size->width(), max_size->height());

  connection()->ScheduleFlush();
}

void WaylandSurface::HandleSurfaceConfigure(int32_t width,
                                            int32_t height,
                                            bool is_maximized,
                                            bool is_fullscreen,
                                            bool is_activated) {
  // Propagate the window state information to the client.
  PlatformWindowState old_state = state_;

  // Ensure that manually handled state changes to fullscreen correspond to the
  // configuration events from a compositor.
  DCHECK_EQ(is_fullscreen, IsFullscreen());

  // There are two cases, which must be handled for the minimized state.
  // The first one is the case, when the surface goes into the minimized state
  // (check comment in WaylandSurface::Minimize), and the second case is when
  // the surface still has been minimized, but another configuration event with
  // !is_activated comes. For this, check if the WaylandSurface has been
  // minimized before and !is_activated is sent.
  if ((is_minimizing_ || IsMinimized()) && !is_activated) {
    is_minimizing_ = false;
    state_ = PlatformWindowState::kMinimized;
  } else if (is_fullscreen) {
    // To ensure the |delegate()| is notified about state changes to fullscreen,
    // assume the old_state is UNKNOWN (check comment in ToggleFullscreen).
    old_state = PlatformWindowState::kUnknown;
    DCHECK(state_ == PlatformWindowState::kFullScreen);
  } else if (is_maximized) {
    state_ = PlatformWindowState::kMaximized;
  } else {
    state_ = PlatformWindowState::kNormal;
  }
  const bool state_changed = old_state != state_;
  const bool is_normal = !IsFullscreen() && !IsMaximized();

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

  if (state_changed) {
    // The |restored_bounds_| are used when the window gets back to normal
    // state after it went maximized or fullscreen.  So we reset these if the
    // window has just become normal and store the current bounds if it is
    // either going out of normal state or simply changes the state and we don't
    // have any meaningful value stored.
    if (is_normal) {
      SetRestoredBoundsInPixels({});
    } else if (old_state == PlatformWindowState::kNormal ||
               GetRestoredBoundsInPixels().IsEmpty()) {
      SetRestoredBoundsInPixels(GetBounds());
    }

    delegate()->OnWindowStateChanged(state_);
  }

  ApplyPendingBounds();

  if (did_active_change)
    delegate()->OnActivationChanged(is_active_);

  MaybeTriggerPendingStateChange();
}

void WaylandSurface::OnDragEnter(const gfx::PointF& point,
                                 std::unique_ptr<OSExchangeData> data,
                                 int operation) {
  WmDropHandler* drop_handler = GetWmDropHandler(*this);
  if (!drop_handler)
    return;
  drop_handler->OnDragEnter(point, std::move(data), operation);
}

int WaylandSurface::OnDragMotion(const gfx::PointF& point,
                                 uint32_t time,
                                 int operation) {
  WmDropHandler* drop_handler = GetWmDropHandler(*this);
  if (!drop_handler)
    return 0;

  return drop_handler->OnDragMotion(point, operation);
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
  CreateShellSurface();
  if (shell_surface_ && !properties.wm_class_class.empty())
    shell_surface_->SetAppId(properties.wm_class_class);
  return !!shell_surface_;
}

bool WaylandSurface::IsMinimized() const {
  return state_ == PlatformWindowState::kMinimized;
}

bool WaylandSurface::IsMaximized() const {
  return state_ == PlatformWindowState::kMaximized;
}

bool WaylandSurface::IsFullscreen() const {
  return state_ == PlatformWindowState::kFullScreen;
}

void WaylandSurface::MaybeTriggerPendingStateChange() {
  if (pending_state_ == PlatformWindowState::kUnknown || !is_active_)
    return;
  DCHECK_EQ(pending_state_, PlatformWindowState::kFullScreen);
  pending_state_ = PlatformWindowState::kUnknown;
  ToggleFullscreen();
}

WmMoveResizeHandler* WaylandSurface::AsWmMoveResizeHandler() {
  return static_cast<WmMoveResizeHandler*>(this);
}

}  // namespace ui
