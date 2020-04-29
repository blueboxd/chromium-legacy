// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/base/x/x11_workspace_handler.h"

#include "base/strings/string_number_conversions.h"
#include "ui/base/x/x11_util.h"
#include "ui/events/x/x11_window_event_manager.h"
#include "ui/gfx/x/x11_atom_cache.h"

namespace ui {

X11WorkspaceHandler::X11WorkspaceHandler(Delegate* delegate)
    : xdisplay_(gfx::GetXDisplay()),
      x_root_window_(DefaultRootWindow(xdisplay_)),
      delegate_(delegate) {
  DCHECK(delegate_);
  if (ui::X11EventSource::HasInstance())
    ui::X11EventSource::GetInstance()->AddXEventDispatcher(this);

  x_root_window_events_ = std::make_unique<ui::XScopedEventSelector>(
      x_root_window_, PropertyChangeMask);
}

X11WorkspaceHandler::~X11WorkspaceHandler() {
  if (ui::X11EventSource::HasInstance())
    ui::X11EventSource::GetInstance()->RemoveXEventDispatcher(this);
}

std::string X11WorkspaceHandler::GetCurrentWorkspace() {
  if (workspace_.empty())
    UpdateWorkspace();
  return workspace_;
}

bool X11WorkspaceHandler::UpdateWorkspace() {
  int desktop;
  if (ui::GetCurrentDesktop(&desktop)) {
    workspace_ = base::NumberToString(desktop);
    return true;
  }
  return false;
}

bool X11WorkspaceHandler::DispatchXEvent(XEvent* event) {
  if (event->type != PropertyNotify ||
      event->xproperty.window != x_root_window_) {
    return false;
  }
  switch (event->type) {
    case PropertyNotify: {
      if (event->xproperty.atom == gfx::GetAtom("_NET_CURRENT_DESKTOP")) {
        if (UpdateWorkspace())
          delegate_->OnCurrentWorkspaceChanged(workspace_);
      }
      break;
    }
    default:
      NOTREACHED();
  }
  return false;
}

}  // namespace ui
