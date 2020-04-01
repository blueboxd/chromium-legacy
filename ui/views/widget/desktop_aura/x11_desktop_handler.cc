// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/widget/desktop_aura/x11_desktop_handler.h"

#include <memory>
#include <string>

#include "ui/aura/env.h"
#include "ui/aura/window_event_dispatcher.h"
#include "ui/base/x/x11_menu_list.h"
#include "ui/base/x/x11_util.h"
#include "ui/events/x/x11_window_event_manager.h"
#include "ui/gfx/x/x11.h"
#include "ui/gfx/x/x11_atom_cache.h"
#include "ui/gfx/x/x11_error_tracker.h"

namespace {

// Our global instance. Deleted when our Env() is deleted.
views::X11DesktopHandler* g_handler = nullptr;

}  // namespace

namespace views {

// static
X11DesktopHandler* X11DesktopHandler::get() {
  if (!g_handler)
    g_handler = new X11DesktopHandler;

  return g_handler;
}

// static
X11DesktopHandler* X11DesktopHandler::get_dont_create() {
  return g_handler;
}

X11DesktopHandler::X11DesktopHandler()
    : xdisplay_(gfx::GetXDisplay()),
      x_root_window_(DefaultRootWindow(xdisplay_)) {
  if (ui::X11EventSource::HasInstance())
    ui::X11EventSource::GetInstance()->AddXEventDispatcher(this);
  aura::Env::GetInstance()->AddObserver(this);

  x_root_window_events_ = std::make_unique<ui::XScopedEventSelector>(
      x_root_window_, StructureNotifyMask | SubstructureNotifyMask);
}

X11DesktopHandler::~X11DesktopHandler() {
  aura::Env::GetInstance()->RemoveObserver(this);
  if (ui::X11EventSource::HasInstance())
    ui::X11EventSource::GetInstance()->RemoveXEventDispatcher(this);
}

bool X11DesktopHandler::DispatchXEvent(XEvent* event) {
  if (event->type != CreateNotify && event->type != DestroyNotify) {
    return false;
  }
  switch (event->type) {
    case CreateNotify:
      OnWindowCreatedOrDestroyed(event->type, event->xcreatewindow.window);
      break;
    case DestroyNotify:
      OnWindowCreatedOrDestroyed(event->type, event->xdestroywindow.window);
      break;
    default:
      NOTREACHED();
  }
  return false;
}

void X11DesktopHandler::OnWindowInitialized(aura::Window* window) {}

void X11DesktopHandler::OnWillDestroyEnv() {
  g_handler = nullptr;
  delete this;
}

void X11DesktopHandler::OnWindowCreatedOrDestroyed(int event_type, XID window) {
  // Menus created by Chrome can be drag and drop targets. Since they are
  // direct children of the screen root window and have override_redirect
  // we cannot use regular _NET_CLIENT_LIST_STACKING property to find them
  // and use a separate cache to keep track of them.
  // TODO(varkha): Implement caching of all top level X windows and their
  // coordinates and stacking order to eliminate repeated calls to the X server
  // during mouse movement, drag and shaping events.
  if (event_type == CreateNotify) {
    // The window might be destroyed if the message pump did not get a chance to
    // run but we can safely ignore the X error.
    gfx::X11ErrorTracker error_tracker;
    ui::XMenuList::GetInstance()->MaybeRegisterMenu(window);
  } else {
    ui::XMenuList::GetInstance()->MaybeUnregisterMenu(window);
  }
}

}  // namespace views
