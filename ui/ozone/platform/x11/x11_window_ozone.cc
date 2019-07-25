// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/ozone/platform/x11/x11_window_ozone.h"

#include <algorithm>
#include <string>
#include <vector>

#include "base/bind.h"
#include "base/strings/utf_string_conversions.h"
#include "ui/base/platform_window_defaults.h"
#include "ui/base/x/x11_util.h"
#include "ui/events/event.h"
#include "ui/events/event_utils.h"
#include "ui/events/ozone/events_ozone.h"
#include "ui/events/platform/x11/x11_event_source.h"
#include "ui/gfx/x/x11.h"
#include "ui/gfx/x/x11_atom_cache.h"
#include "ui/ozone/platform/x11/x11_cursor_ozone.h"
#include "ui/ozone/platform/x11/x11_window_manager_ozone.h"
#include "ui/platform_window/platform_window_init_properties.h"

namespace ui {

namespace {

ui::XWindow::Configuration ConvertInitPropertiesToXWindowConfig(
    const PlatformWindowInitProperties& properties) {
  using WindowType = ui::XWindow::WindowType;
  using WindowOpacity = ui::XWindow::WindowOpacity;
  ui::XWindow::Configuration config;

  switch (properties.type) {
    case PlatformWindowType::kWindow:
      config.type = WindowType::kWindow;
      break;
    case PlatformWindowType::kMenu:
      config.type = WindowType::kMenu;
      break;
    case PlatformWindowType::kTooltip:
      config.type = WindowType::kTooltip;
      break;
    case PlatformWindowType::kPopup:
      config.type = WindowType::kPopup;
      break;
  }

  switch (properties.opacity) {
    case PlatformWindowOpacity::kInferOpacity:
      config.opacity = WindowOpacity::kInferOpacity;
      break;
    case PlatformWindowOpacity::kOpaqueWindow:
      config.opacity = WindowOpacity::kOpaqueWindow;
      break;
    case PlatformWindowOpacity::kTranslucentWindow:
      config.opacity = WindowOpacity::kTranslucentWindow;
      break;
  }

  config.bounds = properties.bounds;

#if defined(OS_CHROMEOS)
  config.activatable = !UseTestConfigForPlatformWindows();
#else
  config.activatable = properties.activatable;
#endif

  // TODO(nickdiego): Add missing platform window init properties
  // config.force_show_in_taskbar = properties.force_show_in_taskbar;
  // config.keep_on_top = properties.keep_on_top;
  // config.visible_on_all_workspaces = properties.visible_on_all_workspaces;
  // config.remove_standard_frame = properties.remove_standard_frame;
  // config.workspace = properties.workspace;
  // config.wm_class_name = properties.wm_class_name;
  // config.wm_class_class = properties.wm_class_class;
  // config.wm_role_name = properties.wm_role_name;

  return config;
}

}  // namespace

X11WindowOzone::X11WindowOzone(PlatformWindowDelegate* delegate,
                               const PlatformWindowInitProperties& properties,
                               X11WindowManagerOzone* window_manager)
    : delegate_(delegate),
      window_manager_(window_manager),
      x11_window_(std::make_unique<ui::XWindow>(this)) {
  DCHECK(delegate_);
  DCHECK(window_manager_);
  Init(properties);
}

X11WindowOzone::~X11WindowOzone() {
  PrepareForShutdown();
  Close();
}

void X11WindowOzone::Init(const PlatformWindowInitProperties& params) {
  XWindow::Configuration config = ConvertInitPropertiesToXWindowConfig(params);
  x11_window_->Init(config);
}

void X11WindowOzone::Show() {
  x11_window_->Map();
}

void X11WindowOzone::Hide() {
  x11_window_->Hide();
}

void X11WindowOzone::Close() {
  if (is_shutting_down_)
    return;

  is_shutting_down_ = true;
  RemoveFromWindowManager();
  SetWidget(x11::None);

  x11_window_->Close();
  delegate_->OnClosed();
}

void X11WindowOzone::SetBounds(const gfx::Rect& bounds) {
  DCHECK(!bounds.size().IsEmpty());

  // Assume that the resize will go through as requested, which should be the
  // case if we're running without a window manager.  If there's a window
  // manager, it can modify or ignore the request, but (per ICCCM) we'll get a
  // (possibly synthetic) ConfigureNotify about the actual size and correct
  // |bounds_| later.
  x11_window_->SetBounds(bounds);

  // Even if the pixel bounds didn't change this call to the delegate should
  // still happen. The device scale factor may have changed which effectively
  // changes the bounds.
  delegate_->OnBoundsChanged(bounds);
}

gfx::Rect X11WindowOzone::GetBounds() {
  return x11_window_->bounds();
}

void X11WindowOzone::SetTitle(const base::string16& title) {
  x11_window_->SetTitle(title);
}

void X11WindowOzone::SetCapture() {
  if (HasCapture())
    return;

  x11_window_->GrabPointer();
  window_manager_->GrabEvents(this);
}

void X11WindowOzone::ReleaseCapture() {
  if (!HasCapture())
    return;

  x11_window_->ReleasePointerGrab();
  window_manager_->UngrabEvents(this);
}

bool X11WindowOzone::HasCapture() const {
  return window_manager_->event_grabber() == this;
}

void X11WindowOzone::OnLostCapture() {
  delegate_->OnLostCapture();
}

void X11WindowOzone::ToggleFullscreen() {
  bool is_fullscreen = state_ == PlatformWindowState::kFullScreen;
  x11_window_->SetFullscreen(!is_fullscreen);
}

void X11WindowOzone::Maximize() {
  if (state_ == PlatformWindowState::kFullScreen)
    ToggleFullscreen();

  x11_window_->Maximize();
}

void X11WindowOzone::Minimize() {
  x11_window_->Minimize();
}

void X11WindowOzone::Restore() {
  if (state_ == PlatformWindowState::kFullScreen)
    ToggleFullscreen();

  if (state_ == PlatformWindowState::kMaximized) {
    x11_window_->Unmaximize();
  }
}

PlatformWindowState X11WindowOzone::GetPlatformWindowState() const {
  return state_;
}

void X11WindowOzone::Activate() {
  x11_window_->Activate();
}

void X11WindowOzone::Deactivate() {
  x11_window_->Deactivate();
}

void X11WindowOzone::MoveCursorTo(const gfx::Point& location) {
  x11_window_->MoveCursorTo(location);
}

void X11WindowOzone::ConfineCursorToBounds(const gfx::Rect& bounds) {
  x11_window_->ConfineCursorTo(bounds);
}

void X11WindowOzone::SetRestoredBoundsInPixels(const gfx::Rect& bounds) {
  // TODO(crbug.com/848131): Restore bounds on restart
  NOTIMPLEMENTED_LOG_ONCE();
}

gfx::Rect X11WindowOzone::GetRestoredBoundsInPixels() const {
  // TODO(crbug.com/848131): Restore bounds on restart
  NOTIMPLEMENTED_LOG_ONCE();
  return gfx::Rect();
}

bool X11WindowOzone::IsVisible() const {
  return x11_window_->IsVisible();
}

gfx::Rect X11WindowOzone::GetOutterBounds() const {
  return x11_window_->GetOutterBounds();
}

::Region X11WindowOzone::GetShape() const {
  return x11_window_->shape();
}

void X11WindowOzone::PrepareForShutdown() {
  DCHECK(X11EventSourceLibevent::GetInstance());
  X11EventSourceLibevent::GetInstance()->RemoveXEventDispatcher(this);
}

void X11WindowOzone::SetCursor(PlatformCursor cursor) {
  X11CursorOzone* cursor_ozone = static_cast<X11CursorOzone*>(cursor);
  x11_window_->SetCursor(cursor_ozone->xcursor());
}

void X11WindowOzone::RemoveFromWindowManager() {
  DCHECK(window_manager_);
  if (widget_ != gfx::kNullAcceleratedWidget) {
    window_manager_->RemoveWindow(this);
  }
}

// CheckCanDispatchNextPlatformEvent is called by X11EventSourceLibevent to
// determine whether X11WindowOzone instance (XEventDispatcher implementation)
// is able to process next translated event sent by it. So, it's done through
// |handle_next_event_| internal flag, used in subsequent CanDispatchEvent
// call.
void X11WindowOzone::CheckCanDispatchNextPlatformEvent(XEvent* xev) {
  if (is_shutting_down_)
    return;

  handle_next_event_ = x11_window_->IsTargetedBy(*xev);
}

void X11WindowOzone::PlatformEventDispatchFinished() {
  handle_next_event_ = false;
}

PlatformEventDispatcher* X11WindowOzone::GetPlatformEventDispatcher() {
  return this;
}

bool X11WindowOzone::DispatchXEvent(XEvent* xev) {
  if (!x11_window_->IsTargetedBy(*xev))
    return false;

  x11_window_->ProcessEvent(xev);
  return true;
}

bool X11WindowOzone::CanDispatchEvent(const PlatformEvent& event) {
  DCHECK_NE(x11_window_->window(), x11::None);
  return handle_next_event_;
}

uint32_t X11WindowOzone::DispatchEvent(const PlatformEvent& event) {
  DCHECK_NE(x11_window_->window(), x11::None);

  if (!window_manager_->event_grabber() ||
      window_manager_->event_grabber() == this) {
    // This is unfortunately needed otherwise events that depend on global state
    // (eg. double click) are broken.
    DispatchEventFromNativeUiEvent(
        event, base::BindOnce(&PlatformWindowDelegate::DispatchEvent,
                              base::Unretained(delegate_)));
    return POST_DISPATCH_STOP_PROPAGATION;
  }

  if (event->IsLocatedEvent()) {
    // Another X11WindowOzone has installed itself as capture. Translate the
    // event's location and dispatch to the other.
    ConvertEventLocationToTargetWindowLocation(
        window_manager_->event_grabber()->GetBounds().origin(),
        GetBounds().origin(), event->AsLocatedEvent());
  }
  return window_manager_->event_grabber()->DispatchEvent(event);
}

void X11WindowOzone::SetWidget(XID xid) {
  // In spite of being defined in Xlib as `unsigned long`, XID (|xid|'s type)
  // is fixed at 32-bits (CARD32) in X11 Protocol, therefore can't be larger
  // than 32 bits values on the wire (see https://crbug.com/607014 for more
  // details). So, It's safe to use static_cast here.
  widget_ = static_cast<gfx::AcceleratedWidget>(xid);
  if (widget_ != gfx::kNullAcceleratedWidget)
    delegate_->OnAcceleratedWidgetAvailable(widget_);
}

void X11WindowOzone::OnXWindowCreated() {
  DCHECK_NE(x11_window_->window(), x11::None);
  SetWidget(x11_window_->window());

  window_manager_->AddWindow(this);

  DCHECK(X11EventSourceLibevent::GetInstance());
  X11EventSourceLibevent::GetInstance()->AddXEventDispatcher(this);
}

void X11WindowOzone::OnXWindowStateChanged() {
  // Propagate the window state information to the client. Note that the order
  // of checks is important here, because window can have several proprties at
  // the same time.
  PlatformWindowState old_state = state_;
  if (x11_window_->IsMinimized()) {
    state_ = PlatformWindowState::kMinimized;
  } else if (x11_window_->IsFullscreen()) {
    state_ = PlatformWindowState::kFullScreen;
  } else if (x11_window_->IsMaximized()) {
    state_ = PlatformWindowState::kMaximized;
  } else {
    state_ = PlatformWindowState::kNormal;
  }

  if (old_state != state_)
    delegate_->OnWindowStateChanged(state_);
}

void X11WindowOzone::OnXWindowDamageEvent(
    const gfx::Rect& damage_rect_in_pixels) {
  delegate_->OnDamageRect(damage_rect_in_pixels);
}

void X11WindowOzone::OnXWindowSizeChanged(const gfx::Size&) {
  delegate_->OnBoundsChanged(x11_window_->bounds());
}

void X11WindowOzone::OnXWindowCloseRequested() {
  delegate_->OnCloseRequest();
}

void X11WindowOzone::OnXWindowLostCapture() {
  OnLostCapture();
}

void X11WindowOzone::OnXWindowIsActiveChanged(bool active) {
  delegate_->OnActivationChanged(active);
}

}  // namespace ui
