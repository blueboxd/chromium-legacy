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

namespace ui {

X11WindowOzone::X11WindowOzone(X11WindowManagerOzone* window_manager,
                               PlatformWindowDelegate* delegate,
                               const gfx::Rect& bounds)
    : window_manager_(window_manager),
      delegate_(delegate),
      xdisplay_(gfx::GetXDisplay()),
      xroot_window_(DefaultRootWindow(xdisplay_)),
      xwindow_(x11::None),
      mapped_(false),
      bounds_(bounds),
      state_(PlatformWindowState::kUnknown) {
  DCHECK(delegate_);
  DCHECK(window_manager_);
  Create();
}

X11WindowOzone::~X11WindowOzone() {
  PrepareForShutdown();
  UnconfineCursor();
  Destroy();
}

void X11WindowOzone::Destroy() {
  if (xwindow_ == x11::None)
    return;

  RemoveFromWindowManager();

  // Stop processing events.
  XID xwindow = xwindow_;
  XDisplay* xdisplay = xdisplay_;
  SetXWindow(x11::None);

  delegate_->OnClosed();
  XDestroyWindow(xdisplay, xwindow);
}

void X11WindowOzone::Create() {
  DCHECK(!bounds_.size().IsEmpty());
  DCHECK_NE(xdisplay_, nullptr);

  XID xwindow = CreateXWindow();
  DCHECK_NE(xwindow, x11::None);

  SetXWindow(xwindow);
  window_manager_->AddWindow(this);

  DCHECK(X11EventSourceLibevent::GetInstance());
  X11EventSourceLibevent::GetInstance()->AddXEventDispatcher(this);
}

XID X11WindowOzone::CreateXWindow() {
  XID xwindow = x11::None;
  XSetWindowAttributes swa;
  memset(&swa, 0, sizeof(swa));
  swa.background_pixmap = x11::None;
  swa.bit_gravity = NorthWestGravity;
  swa.override_redirect = UseTestConfigForPlatformWindows();
  xwindow =
      XCreateWindow(xdisplay_, xroot_window_, bounds_.x(), bounds_.y(),
                    bounds_.width(), bounds_.height(),
                    0,               // border width
                    CopyFromParent,  // depth
                    InputOutput,
                    CopyFromParent,  // visual
                    CWBackPixmap | CWBitGravity | CWOverrideRedirect, &swa);

  // Setup XInput event mask.
  long event_mask = ButtonPressMask | ButtonReleaseMask | FocusChangeMask |
                    KeyPressMask | KeyReleaseMask | EnterWindowMask |
                    LeaveWindowMask | ExposureMask | VisibilityChangeMask |
                    StructureNotifyMask | PropertyChangeMask |
                    PointerMotionMask;
  xwindow_events_.reset(new ui::XScopedEventSelector(xwindow, event_mask));

  // Setup XInput2 event mask.
  unsigned char mask[XIMaskLen(XI_LASTEVENT)];
  memset(mask, 0, sizeof(mask));

  XISetMask(mask, XI_TouchBegin);
  XISetMask(mask, XI_TouchUpdate);
  XISetMask(mask, XI_TouchEnd);
  XISetMask(mask, XI_ButtonPress);
  XISetMask(mask, XI_ButtonRelease);
  XISetMask(mask, XI_Motion);
  XISetMask(mask, XI_KeyPress);
  XISetMask(mask, XI_KeyRelease);
  XISetMask(mask, XI_HierarchyChanged);

  XIEventMask evmask;
  evmask.deviceid = XIAllDevices;
  evmask.mask_len = sizeof(mask);
  evmask.mask = mask;
  XISelectEvents(xdisplay_, xwindow, &evmask, 1);
  XFlush(xdisplay_);

  XAtom protocols[] = {gfx::GetAtom("WM_DELETE_WINDOW"),
                       gfx::GetAtom("_NET_WM_PING")};
  XSetWMProtocols(xdisplay_, xwindow, protocols, 2);

  // We need a WM_CLIENT_MACHINE and WM_LOCALE_NAME value so we integrate with
  // the desktop environment.
  XSetWMProperties(xdisplay_, xwindow, NULL, NULL, NULL, 0, NULL, NULL, NULL);

  // Likewise, the X server needs to know this window's pid so it knows which
  // program to kill if the window hangs. XChangeProperty() expects "pid" to be
  // long.
  static_assert(sizeof(long) >= sizeof(pid_t),
                "pid_t should not be larger than long");
  long pid = getpid();
  XChangeProperty(xdisplay_, xwindow, gfx::GetAtom("_NET_WM_PID"), XA_CARDINAL,
                  32, PropModeReplace, reinterpret_cast<unsigned char*>(&pid),
                  1);
  // Before we map the window, set size hints. Otherwise, some window managers
  // will ignore toplevel XMoveWindow commands.
  XSizeHints size_hints;
  size_hints.flags = PPosition | PWinGravity;
  size_hints.x = bounds_.x();
  size_hints.y = bounds_.y();
  // Set StaticGravity so that the window position is not affected by the
  // frame width when running with window manager.
  size_hints.win_gravity = StaticGravity;
  XSetWMNormalHints(xdisplay_, xwindow, &size_hints);

  return xwindow;
}

void X11WindowOzone::Show() {
  if (mapped_)
    return;

  XMapWindow(xdisplay_, xwindow_);
  XFlush(xdisplay_);
  mapped_ = true;
}

void X11WindowOzone::Hide() {
  if (!mapped_)
    return;

  XWithdrawWindow(xdisplay_, xwindow_, 0);
  mapped_ = false;
}

void X11WindowOzone::Close() {
  Destroy();
}

void X11WindowOzone::SetBounds(const gfx::Rect& bounds) {
  DCHECK(!bounds.size().IsEmpty());

  if (xwindow_ != x11::None) {
    XWindowChanges changes = {0};
    unsigned value_mask = 0;

    if (bounds_.size() != bounds.size()) {
      changes.width = bounds.width();
      changes.height = bounds.height();
      value_mask |= CWHeight | CWWidth;
    }

    if (bounds_.origin() != bounds.origin()) {
      changes.x = bounds.x();
      changes.y = bounds.y();
      value_mask |= CWX | CWY;
    }

    if (value_mask)
      XConfigureWindow(xdisplay_, xwindow_, value_mask, &changes);
  }

  // Assume that the resize will go through as requested, which should be the
  // case if we're running without a window manager.  If there's a window
  // manager, it can modify or ignore the request, but (per ICCCM) we'll get a
  // (possibly synthetic) ConfigureNotify about the actual size and correct
  // |bounds_| later.
  bounds_ = bounds;

  // Even if the pixel bounds didn't change this call to the delegate should
  // still happen. The device scale factor may have changed which effectively
  // changes the bounds.
  delegate_->OnBoundsChanged(bounds_);
}

gfx::Rect X11WindowOzone::GetBounds() {
  return bounds_;
}

void X11WindowOzone::SetTitle(const base::string16& title) {
  if (window_title_ == title)
    return;

  window_title_ = title;
  std::string utf8str = base::UTF16ToUTF8(title);
  XChangeProperty(xdisplay_, xwindow_, gfx::GetAtom("_NET_WM_NAME"),
                  gfx::GetAtom("UTF8_STRING"), 8, PropModeReplace,
                  reinterpret_cast<const unsigned char*>(utf8str.c_str()),
                  utf8str.size());
  XTextProperty xtp;
  char* c_utf8_str = const_cast<char*>(utf8str.c_str());
  if (Xutf8TextListToTextProperty(xdisplay_, &c_utf8_str, 1, XUTF8StringStyle,
                                  &xtp) == x11::Success) {
    XSetWMName(xdisplay_, xwindow_, &xtp);
    XFree(xtp.value);
  }
}

void X11WindowOzone::SetCapture() {
  window_manager_->GrabEvents(this);
}

void X11WindowOzone::ReleaseCapture() {
  window_manager_->UngrabEvents(this);
}

bool X11WindowOzone::HasCapture() const {
  return window_manager_->event_grabber() == this;
}

void X11WindowOzone::ToggleFullscreen() {
  bool is_fullscreen = state_ == PlatformWindowState::kFullScreen;
  ui::SetWMSpecState(xwindow_, !is_fullscreen,
                     gfx::GetAtom("_NET_WM_STATE_FULLSCREEN"), x11::None);
}

void X11WindowOzone::Maximize() {
  if (state_ == PlatformWindowState::kFullScreen)
    ToggleFullscreen();

  ui::SetWMSpecState(xwindow_, true,
                     gfx::GetAtom("_NET_WM_STATE_MAXIMIZED_VERT"),
                     gfx::GetAtom("_NET_WM_STATE_MAXIMIZED_HORZ"));
}

void X11WindowOzone::Minimize() {
  XIconifyWindow(xdisplay_, xwindow_, 0);
}

void X11WindowOzone::Restore() {
  if (state_ == PlatformWindowState::kFullScreen)
    ToggleFullscreen();

  if (state_ == PlatformWindowState::kMaximized) {
    ui::SetWMSpecState(xwindow_, false,
                       gfx::GetAtom("_NET_WM_STATE_MAXIMIZED_VERT"),
                       gfx::GetAtom("_NET_WM_STATE_MAXIMIZED_HORZ"));
  }
}

PlatformWindowState X11WindowOzone::GetPlatformWindowState() const {
  return state_;
}

void X11WindowOzone::Activate() {
  // TODO(crbug.com/973772): Propagate activation changes
  NOTIMPLEMENTED_LOG_ONCE();
}

void X11WindowOzone::Deactivate() {
  // TODO(crbug.com/973772): Propagate activation changes
  NOTIMPLEMENTED_LOG_ONCE();
}

void X11WindowOzone::MoveCursorTo(const gfx::Point& location) {
  XWarpPointer(xdisplay_, x11::None, xroot_window_, 0, 0, 0, 0,
               bounds_.x() + location.x(), bounds_.y() + location.y());
}

void X11WindowOzone::ConfineCursorToBounds(const gfx::Rect& bounds) {
  UnconfineCursor();

  if (bounds.IsEmpty())
    return;

  gfx::Rect barrier = bounds + bounds_.OffsetFromOrigin();

  // Top horizontal barrier.
  pointer_barriers_[0] = XFixesCreatePointerBarrier(
      xdisplay_, xroot_window_, barrier.x(), barrier.y(), barrier.right(),
      barrier.y(), BarrierPositiveY, 0, XIAllDevices);
  // Bottom horizontal barrier.
  pointer_barriers_[1] = XFixesCreatePointerBarrier(
      xdisplay_, xroot_window_, barrier.x(), barrier.bottom(), barrier.right(),
      barrier.bottom(), BarrierNegativeY, 0, XIAllDevices);
  // Left vertical barrier.
  pointer_barriers_[2] = XFixesCreatePointerBarrier(
      xdisplay_, xroot_window_, barrier.x(), barrier.y(), barrier.x(),
      barrier.bottom(), BarrierPositiveX, 0, XIAllDevices);
  // Right vertical barrier.
  pointer_barriers_[3] = XFixesCreatePointerBarrier(
      xdisplay_, xroot_window_, barrier.right(), barrier.y(), barrier.right(),
      barrier.bottom(), BarrierNegativeX, 0, XIAllDevices);

  has_pointer_barriers_ = true;
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

void X11WindowOzone::UnconfineCursor() {
  if (!has_pointer_barriers_)
    return;

  for (XID pointer_barrier : pointer_barriers_)
    XFixesDestroyPointerBarrier(xdisplay_, pointer_barrier);
  pointer_barriers_.fill(x11::None);

  has_pointer_barriers_ = false;
}

void X11WindowOzone::PrepareForShutdown() {
  DCHECK(X11EventSourceLibevent::GetInstance());
  X11EventSourceLibevent::GetInstance()->RemoveXEventDispatcher(this);
}

void X11WindowOzone::SetCursor(PlatformCursor cursor) {
  X11CursorOzone* cursor_ozone = static_cast<X11CursorOzone*>(cursor);
  XDefineCursor(xdisplay_, xwindow_, cursor_ozone->xcursor());
}

void X11WindowOzone::RemoveFromWindowManager() {
  DCHECK(window_manager_);
  if (xwindow_ != x11::None) {
    window_manager_->RemoveWindow(this);
  }
}

// CheckCanDispatchNextPlatformEvent is called by X11EventSourceLibevent to
// determine whether X11WindowOzone instance (XEventDispatcher implementation)
// is able to process next translated event sent by it. So, it's done through
// |handle_next_event_| internal flag, used in subsequent CanDispatchEvent
// call.
void X11WindowOzone::CheckCanDispatchNextPlatformEvent(XEvent* xev) {
  handle_next_event_ = xwindow_ != x11::None && IsEventForXWindow(*xev);
}

void X11WindowOzone::PlatformEventDispatchFinished() {
  handle_next_event_ = false;
}

PlatformEventDispatcher* X11WindowOzone::GetPlatformEventDispatcher() {
  return this;
}

bool X11WindowOzone::DispatchXEvent(XEvent* xev) {
  if (!IsEventForXWindow(*xev))
    return false;

  ProcessXWindowEvent(xev);
  return true;
}

bool X11WindowOzone::CanDispatchEvent(const PlatformEvent& event) {
  DCHECK_NE(xwindow_, x11::None);
  return handle_next_event_;
}

uint32_t X11WindowOzone::DispatchEvent(const PlatformEvent& event) {
  DCHECK_NE(xwindow_, x11::None);
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

void X11WindowOzone::OnLostCapture() {
  delegate_->OnLostCapture();
}

void X11WindowOzone::SetXWindow(XID xid) {
  xwindow_ = xid;

  // In spite of being defined in Xlib as `unsigned long`, XID (|xwindow_|'s
  // type) is fixed at 32-bits (CARD32) in X11 Protocol, therefore can't be
  // larger than 32 bits values on the wire (see https://crbug.com/607014 for
  // more details). So, It's safe to use static_cast here.
  widget_ = static_cast<gfx::AcceleratedWidget>(xwindow_);
  if (widget_ != gfx::kNullAcceleratedWidget)
    delegate_->OnAcceleratedWidgetAvailable(widget_);
}

bool X11WindowOzone::IsEventForXWindow(const XEvent& xev) const {
  XID target_window = (xev.type == GenericEvent)
                          ? static_cast<XIDeviceEvent*>(xev.xcookie.data)->event
                          : xev.xany.window;
  return target_window == xwindow_;
}

void X11WindowOzone::ProcessXWindowEvent(XEvent* xev) {
  switch (xev->type) {
    case Expose: {
      gfx::Rect damage_rect(xev->xexpose.x, xev->xexpose.y, xev->xexpose.width,
                            xev->xexpose.height);
      delegate_->OnDamageRect(damage_rect);
      break;
    }

    case x11::FocusOut:
      if (xev->xfocus.mode != NotifyGrab)
        delegate_->OnLostCapture();
      break;

    case ConfigureNotify: {
      DCHECK_EQ(xwindow_, xev->xconfigure.event);
      DCHECK_EQ(xwindow_, xev->xconfigure.window);
      // It's possible that the X window may be resized by some other means than
      // from within aura (e.g. the X window manager can change the size). Make
      // sure the root window size is maintained properly.
      int translated_x_in_pixels = xev->xconfigure.x;
      int translated_y_in_pixels = xev->xconfigure.y;
      if (!xev->xconfigure.send_event && !xev->xconfigure.override_redirect) {
        Window unused;
        XTranslateCoordinates(xdisplay_, xwindow_, xroot_window_, 0, 0,
                              &translated_x_in_pixels, &translated_y_in_pixels,
                              &unused);
      }
      gfx::Rect bounds(translated_x_in_pixels, translated_y_in_pixels,
                       xev->xconfigure.width, xev->xconfigure.height);
      if (bounds_ != bounds) {
        bounds_ = bounds;
        delegate_->OnBoundsChanged(bounds_);
      }
      break;
    }

    case ClientMessage: {
      Atom message = static_cast<Atom>(xev->xclient.data.l[0]);
      if (message == gfx::GetAtom("WM_DELETE_WINDOW")) {
        delegate_->OnCloseRequest();
      } else if (message == gfx::GetAtom("_NET_WM_PING")) {
        XEvent reply_event = *xev;
        reply_event.xclient.window = xroot_window_;

        XSendEvent(xdisplay_, reply_event.xclient.window, x11::False,
                   SubstructureRedirectMask | SubstructureNotifyMask,
                   &reply_event);
        XFlush(xdisplay_);
      }
      break;
    }
    case PropertyNotify: {
      XAtom changed_atom = xev->xproperty.atom;
      if (changed_atom == gfx::GetAtom("_NET_WM_STATE"))
        OnWMStateUpdated();
      break;
    }
  }
}

void X11WindowOzone::OnWMStateUpdated() {
  std::vector<XAtom> atom_list;
  // Ignore the return value of ui::GetAtomArrayProperty(). Fluxbox removes the
  // _NET_WM_STATE property when no _NET_WM_STATE atoms are set.
  ui::GetAtomArrayProperty(xwindow_, "_NET_WM_STATE", &atom_list);

  window_properties_.clear();
  std::copy(atom_list.begin(), atom_list.end(),
            inserter(window_properties_, window_properties_.begin()));

  // Propagate the window state information to the client.
  // Note that the order of checks is important here, because window can have
  // several proprties at the same time.
  PlatformWindowState old_state = state_;
  if (ui::HasWMSpecProperty(window_properties_,
                            gfx::GetAtom("_NET_WM_STATE_HIDDEN"))) {
    state_ = PlatformWindowState::kMinimized;
  } else if (ui::HasWMSpecProperty(window_properties_,
                                   gfx::GetAtom("_NET_WM_STATE_FULLSCREEN"))) {
    state_ = PlatformWindowState::kFullScreen;
  } else if (ui::HasWMSpecProperty(
                 window_properties_,
                 gfx::GetAtom("_NET_WM_STATE_MAXIMIZED_VERT")) &&
             ui::HasWMSpecProperty(
                 window_properties_,
                 gfx::GetAtom("_NET_WM_STATE_MAXIMIZED_HORZ"))) {
    state_ = PlatformWindowState::kMaximized;
  } else {
    state_ = PlatformWindowState::kNormal;
  }

  if (old_state != state_)
    delegate_->OnWindowStateChanged(state_);
}

}  // namespace ui
