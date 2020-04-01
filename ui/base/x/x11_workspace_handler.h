// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_BASE_X_X11_WORKSPACE_HANDLER_H_
#define UI_BASE_X_X11_WORKSPACE_HANDLER_H_

#include <memory>

#include "base/component_export.h"
#include "ui/events/platform/x11/x11_event_source.h"
#include "ui/gfx/x/x11.h"

namespace ui {

class XScopedEventSelector;

// Listens for global workspace changes and notifies observers.
class COMPONENT_EXPORT(UI_BASE_X) X11WorkspaceHandler
    : public ui::XEventDispatcher {
 public:
  class Delegate {
   public:
    // Called when the workspace ID changes to|new_workspace|.
    virtual void OnCurrentWorkspaceChanged(
        const std::string& new_workspace) = 0;

   protected:
    virtual ~Delegate() = default;
  };
  explicit X11WorkspaceHandler(Delegate* delegate);
  ~X11WorkspaceHandler() override;
  X11WorkspaceHandler(const X11WorkspaceHandler&) = delete;
  X11WorkspaceHandler& operator=(const X11WorkspaceHandler&) = delete;

  // Gets the current workspace ID.
  std::string GetCurrentWorkspace();

 private:
  // ui::XEventDispatcher
  bool DispatchXEvent(XEvent* event) override;

  // Makes a round trip to the X server to get the current workspace.
  bool UpdateWorkspace();

  // The display and the native X window hosting the root window.
  XDisplay* xdisplay_;

  // The native root window.
  ::Window x_root_window_;

  // Events selected on x_root_window_.
  std::unique_ptr<ui::XScopedEventSelector> x_root_window_events_;

  std::string workspace_;

  Delegate* const delegate_;
};

}  // namespace ui

#endif  // UI_BASE_X_X11_WORKSPACE_HANDLER_H_
