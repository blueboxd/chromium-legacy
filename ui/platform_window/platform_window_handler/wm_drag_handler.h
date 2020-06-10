// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_PLATFORM_WINDOW_PLATFORM_WINDOW_HANDLER_WM_DRAG_HANDLER_H_
#define UI_PLATFORM_WINDOW_PLATFORM_WINDOW_HANDLER_WM_DRAG_HANDLER_H_

#include "base/bind.h"
#include "ui/base/dragdrop/drag_drop_types.h"
#include "ui/gfx/geometry/point.h"
#include "ui/gfx/native_widget_types.h"
#include "ui/platform_window/platform_window_handler/wm_platform_export.h"

namespace ui {
class PlatformWindow;
class OSExchangeData;

class WM_PLATFORM_EXPORT WmDragHandler {
 public:
  // During the drag operation, the handler may send updates
  class Delegate {
   public:
    // Called every time when the drag location has changed.
    virtual void OnDragLocationChanged(const gfx::Point& screen_point_px) = 0;
    // Called when the currently negotiated operation has changed.
    virtual void OnDragOperationChanged(
        DragDropTypes::DragOperation operation) = 0;
    // Called once when the operation has finished.
    virtual void OnDragFinished(int operation) = 0;

   protected:
    virtual ~Delegate();
  };

  // Starts dragging with |data| which it wants to deliver to the destination.
  // |operation| is the suggested operation which is bitmask of DRAG_NONE,
  // DRAG_MOVE, DRAG_COPY and DRAG_LINK in DragDropTypes::DragOperation to the
  // destination and the destination sets the final operation when the drop
  // action is performed.  In progress updates on the drag operation come back
  // through the |delegate|.
  virtual void StartDrag(const OSExchangeData& data,
                         int operation,
                         gfx::NativeCursor cursor,
                         Delegate* delegate) = 0;

 protected:
  virtual ~WmDragHandler() {}
};

WM_PLATFORM_EXPORT void SetWmDragHandler(PlatformWindow* platform_window,
                                         WmDragHandler* drag_handler);
WM_PLATFORM_EXPORT WmDragHandler* GetWmDragHandler(
    const PlatformWindow& platform_window);

}  // namespace ui

#endif  // UI_PLATFORM_WINDOW_PLATFORM_WINDOW_HANDLER_WM_DRAG_HANDLER_H_
