// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_PLATFORM_WINDOW_PLATFORM_WINDOW_HANDLER_WM_MOVE_LOOP_HANDLER_H_
#define UI_PLATFORM_WINDOW_PLATFORM_WINDOW_HANDLER_WM_MOVE_LOOP_HANDLER_H_

#include "ui/platform_window/platform_window_handler/wm_platform_export.h"

namespace gfx {
class Vector2d;
}

namespace ui {

class PlatformWindow;

// Handler that starts interactive move loop for the PlatformWindow.
class WM_PLATFORM_EXPORT WmMoveLoopHandler {
 public:
  // Starts a move loop for tab drag controller. Returns true on success or
  // false on fail/cancel.
  virtual bool RunMoveLoop(const gfx::Vector2d& drag_offset) = 0;

  // Ends the move loop.
  virtual void EndMoveLoop() = 0;

 protected:
  virtual ~WmMoveLoopHandler() {}
};

WM_PLATFORM_EXPORT void SetWmMoveLoopHandler(PlatformWindow* platform_window,
                                             WmMoveLoopHandler* drag_handler);
WM_PLATFORM_EXPORT WmMoveLoopHandler* GetWmMoveLoopHandler(
    const PlatformWindow& platform_window);

}  // namespace ui

#endif  // UI_PLATFORM_WINDOW_PLATFORM_WINDOW_HANDLER_WM_MOVE_LOOP_HANDLER_H_
