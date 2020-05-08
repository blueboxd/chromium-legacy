// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_OZONE_PLATFORM_X11_X11_WINDOW_OZONE_H_
#define UI_OZONE_PLATFORM_X11_X11_WINDOW_OZONE_H_

#include "ui/base/x/x11_drag_drop_client.h"
#include "ui/platform_window/platform_window_handler/wm_drag_handler.h"
#include "ui/platform_window/x11/x11_window.h"

namespace ui {

class PlatformWindowDelegate;

// PlatformWindow implementation for non-ChromeOS X11 Ozone.
// PlatformEvents are ui::Events.
class X11WindowOzone : public X11Window,
                       public WmDragHandler,
                       public XDragDropClient::Delegate {
 public:
  explicit X11WindowOzone(PlatformWindowDelegate* delegate);
  ~X11WindowOzone() override;

  X11WindowOzone(const X11WindowOzone&) = delete;
  X11WindowOzone& operator=(const X11WindowOzone&) = delete;

  // Overridden from PlatformWindow:
  void SetCursor(PlatformCursor cursor) override;

  // Overridden from X11Window:
  void Initialize(PlatformWindowInitProperties properties) override;

 private:
  // WmDragHandler
  void StartDrag(const ui::OSExchangeData& data,
                 int operation,
                 gfx::NativeCursor cursor,
                 base::OnceCallback<void(int)> callback) override;

  // ui::XDragDropClient::Delegate
  std::unique_ptr<ui::XTopmostWindowFinder> CreateWindowFinder() override;
  int UpdateDrag(const gfx::Point& screen_point) override;
  void UpdateCursor(
      ui::DragDropTypes::DragOperation negotiated_operation) override;
  void OnBeginForeignDrag(XID window) override;
  void OnEndForeignDrag() override;
  void OnBeforeDragLeave() override;
  int PerformDrop() override;
  void EndMoveLoop() override;

  // X11Window:
  bool DispatchDraggingUiEvent(ui::Event* event) override;
  void OnXWindowSelectionEvent(XEvent* xev) override;
  void OnXWindowDragDropEvent(XEvent* xev) override;

  // True while the drag initiated in this window is in progress.
  bool dragging_ = false;

  std::unique_ptr<XDragDropClient> drag_drop_client_;
  base::OnceCallback<void(int)> end_drag_callback_;
};

}  // namespace ui

#endif  // UI_OZONE_PLATFORM_X11_X11_WINDOW_OZONE_H_
