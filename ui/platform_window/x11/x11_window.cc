// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/platform_window/x11/x11_window.h"

#include "base/trace_event/trace_event.h"
#include "ui/events/devices/x11/touch_factory_x11.h"
#include "ui/events/event.h"
#include "ui/events/event_utils.h"
#include "ui/events/platform/platform_event_source.h"
#include "ui/events/platform/x11/x11_event_source.h"
#include "ui/gfx/x/x11.h"
#include "ui/platform_window/platform_window_delegate_linux.h"

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
    case PlatformWindowType::kDrag:
      config.type = WindowType::kDrag;
      break;
    case PlatformWindowType::kBubble:
      config.type = WindowType::kBubble;
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
  config.force_show_in_taskbar = properties.force_show_in_taskbar;
  config.keep_on_top = properties.keep_on_top;
  config.visible_on_all_workspaces = properties.visible_on_all_workspaces;
  config.remove_standard_frame = properties.remove_standard_frame;
  config.workspace = properties.workspace;
  config.wm_class_name = properties.wm_class_name;
  config.wm_class_class = properties.wm_class_class;
  config.wm_role_name = properties.wm_role_name;
  config.activatable = properties.activatable;
  return config;
}

}  // namespace

X11Window::X11Window(PlatformWindowDelegateLinux* platform_window_delegate)
    : platform_window_delegate_(platform_window_delegate) {}

X11Window::~X11Window() {
  PrepareForShutdown();
  Close();
}

void X11Window::Initialize(PlatformWindowInitProperties properties) {
  XWindow::Configuration config =
      ConvertInitPropertiesToXWindowConfig(properties);
  Init(config);
}

void X11Window::SetXEventDelegate(XEventDelegate* delegate) {
  DCHECK(!x_event_delegate_);
  x_event_delegate_ = delegate;
}

void X11Window::Show() {
  // TODO(msisov): pass inactivity to PlatformWindow::Show.
  XWindow::Map(false /* inactive */);
}

void X11Window::Hide() {
  XWindow::Hide();
}

void X11Window::Close() {
  if (is_shutting_down_)
    return;

  is_shutting_down_ = true;
  XWindow::Close();
  platform_window_delegate_->OnClosed();
}

void X11Window::PrepareForShutdown() {
  PlatformEventSource::GetInstance()->RemovePlatformEventDispatcher(this);
}

void X11Window::SetBounds(const gfx::Rect& bounds) {
  // Assume that the resize will go through as requested, which should be the
  // case if we're running without a window manager.  If there's a window
  // manager, it can modify or ignore the request, but (per ICCCM) we'll get a
  // (possibly synthetic) ConfigureNotify about the actual size and correct
  // |bounds_| later.
  XWindow::SetBounds(bounds);

  // Even if the pixel bounds didn't change this call to the delegate should
  // still happen. The device scale factor may have changed which effectively
  // changes the bounds.
  platform_window_delegate_->OnBoundsChanged(bounds);
}

gfx::Rect X11Window::GetBounds() {
  return XWindow::bounds();
}

void X11Window::SetTitle(const base::string16& title) {
  XWindow::SetTitle(title);
}

void X11Window::SetCapture() {
  XWindow::GrabPointer();
}

void X11Window::ReleaseCapture() {
  XWindow::ReleasePointerGrab();
}

bool X11Window::HasCapture() const {
  NOTIMPLEMENTED_LOG_ONCE();
  return false;
}

void X11Window::ToggleFullscreen() {
  bool is_fullscreen = IsFullscreen();
  SetFullscreen(!is_fullscreen);
}

void X11Window::Maximize() {
  if (IsFullscreen())
    SetFullscreen(false);
  XWindow::Maximize();
}

void X11Window::Minimize() {
  XWindow::Minimize();
}

void X11Window::Restore() {
  if (IsFullscreen())
    ToggleFullscreen();
  if (IsMaximized())
    Unmaximize();
}

PlatformWindowState X11Window::GetPlatformWindowState() const {
  return state_;
}

void X11Window::Activate() {
  XWindow::Activate();
}

void X11Window::Deactivate() {
  XWindow::Deactivate();
}

void X11Window::SetUseNativeFrame(bool use_native_frame) {
  XWindow::SetUseNativeFrame(use_native_frame);
}

void X11Window::SetCursor(PlatformCursor cursor) {
  // X11PlatformWindowOzone has different type of PlatformCursor. Thus, use this
  // only for X11 and Ozone will manage this by itself.
#if defined(USE_X11)
  XWindow::SetCursor(cursor);
#endif
}

void X11Window::MoveCursorTo(const gfx::Point& location) {
  XWindow::MoveCursorTo(location);
}

void X11Window::ConfineCursorToBounds(const gfx::Rect& bounds) {
  XWindow::ConfineCursorTo(bounds);
}

void X11Window::SetRestoredBoundsInPixels(const gfx::Rect& bounds) {
  // TODO(crbug.com/848131): Restore bounds on restart
  NOTIMPLEMENTED_LOG_ONCE();
}

gfx::Rect X11Window::GetRestoredBoundsInPixels() const {
  // TODO(crbug.com/848131): Restore bounds on restart
  NOTIMPLEMENTED_LOG_ONCE();
  return gfx::Rect();
}

void X11Window::SetPlatformEventDispatcher() {
  DCHECK(PlatformEventSource::GetInstance());
  PlatformEventSource::GetInstance()->AddPlatformEventDispatcher(this);
}

bool X11Window::CanDispatchEvent(const PlatformEvent& xev) {
#if defined(USE_X11)
  return XWindow::IsTargetedBy(*xev);
#else
  NOTREACHED() << "Ozone must use own dispatcher as it has different type of "
                  "PlatformEvent";
  return false;
#endif
}

uint32_t X11Window::DispatchEvent(const PlatformEvent& event) {
#if defined(USE_X11)
  TRACE_EVENT1("views", "X11PlatformWindow::Dispatch", "event->type",
               event->type);

  ProcessEvent(event);
  return POST_DISPATCH_STOP_PROPAGATION;
#else
  NOTREACHED() << "Ozone must use own dispatcher as it has different type of "
                  "PlatformEvent";
  return false;
#endif
}

void X11Window::OnXWindowCreated() {
  // X11WindowOzone overrides this method and manages events by itself.
  SetPlatformEventDispatcher();
  platform_window_delegate_->OnAcceleratedWidgetAvailable(window());
}

void X11Window::OnXWindowStateChanged() {
  // Propagate the window state information to the client. Note that the order
  // of checks is important here, because window can have several properties
  // at the same time.
  PlatformWindowState old_state = state_;
  if (IsMinimized()) {
    state_ = PlatformWindowState::kMinimized;
  } else if (IsFullscreen()) {
    state_ = PlatformWindowState::kFullScreen;
  } else if (IsMaximized()) {
    state_ = PlatformWindowState::kMaximized;
  } else {
    state_ = PlatformWindowState::kNormal;
  }

  if (old_state != state_)
    platform_window_delegate_->OnWindowStateChanged(state_);
}

void X11Window::OnXWindowDamageEvent(const gfx::Rect& damage_rect) {
  platform_window_delegate_->OnDamageRect(damage_rect);
}

void X11Window::OnXWindowBoundsChanged(const gfx::Rect& bounds) {
  platform_window_delegate_->OnBoundsChanged(bounds);
}

void X11Window::OnXWindowCloseRequested() {
  platform_window_delegate_->OnCloseRequest();
}

void X11Window::OnXWindowIsActiveChanged(bool active) {
  platform_window_delegate_->OnActivationChanged(active);
}

void X11Window::OnXWindowMapped() {
  platform_window_delegate_->OnXWindowMapped();
}

void X11Window::OnXWindowUnmapped() {
  platform_window_delegate_->OnXWindowUnmapped();
}

void X11Window::OnXWindowWorkspaceChanged() {
  platform_window_delegate_->OnWorkspaceChanged();
}

void X11Window::OnXWindowLostPointerGrab() {
  platform_window_delegate_->OnLostMouseGrab();
}

void X11Window::OnXWindowLostCapture() {
  platform_window_delegate_->OnLostCapture();
}

void X11Window::OnXWindowEvent(ui::Event* event) {
  platform_window_delegate_->DispatchEvent(event);
}

void X11Window::OnXWindowSelectionEvent(XEvent* xev) {
  if (x_event_delegate_)
    x_event_delegate_->OnXWindowSelectionEvent(xev);
}

void X11Window::OnXWindowDragDropEvent(XEvent* xev) {
  if (x_event_delegate_)
    x_event_delegate_->OnXWindowDragDropEvent(xev);
}

void X11Window::OnXWindowRawKeyEvent(XEvent* xev) {
  if (x_event_delegate_)
    x_event_delegate_->OnXWindowRawKeyEvent(xev);
}

base::Optional<gfx::Size> X11Window::GetMinimumSizeForXWindow() {
  return platform_window_delegate_->GetMinimumSizeForWindow();
}

base::Optional<gfx::Size> X11Window::GetMaximumSizeForXWindow() {
  return platform_window_delegate_->GetMaximumSizeForWindow();
}

}  // namespace ui
