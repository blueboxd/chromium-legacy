// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_VIEWS_WIDGET_DESKTOP_AURA_DESKTOP_DRAG_DROP_CLIENT_AURAX11_H_
#define UI_VIEWS_WIDGET_DESKTOP_AURA_DESKTOP_DRAG_DROP_CLIENT_AURAX11_H_

#include <memory>
#include <set>
#include <vector>

#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "ui/aura/client/drag_drop_client.h"
#include "ui/aura/window_observer.h"
#include "ui/base/cursor/cursor.h"
#include "ui/base/dragdrop/drag_drop_types.h"
#include "ui/base/x/x11_drag_drop_client.h"
#include "ui/events/event_constants.h"
#include "ui/events/platform/x11/x11_event_source.h"
#include "ui/events/x/x11_window_event_manager.h"
#include "ui/gfx/geometry/point.h"
#include "ui/gfx/geometry/size.h"
#include "ui/gfx/x/x11.h"
#include "ui/views/views_export.h"
#include "ui/views/widget/desktop_aura/x11_move_loop_delegate.h"

namespace aura {
namespace client {
class DragDropClientObserver;
class DragDropDelegate;
}  // namespace client
}  // namespace aura

namespace gfx {
class Point;
}

namespace ui {
class DropTargetEvent;
class OSExchangeData;
class XTopmostWindowFinder;
}  // namespace ui

namespace views {
class DesktopNativeCursorManager;
class Widget;
class X11MoveLoop;

// Implements drag and drop on X11 for aura. On one side, this class takes raw
// X11 events forwarded from DesktopWindowTreeHostLinux, while on the other, it
// handles the views drag events.
class VIEWS_EXPORT DesktopDragDropClientAuraX11
    : public ui::XDragDropClient,
      public ui::XDragDropClient::Delegate,
      public aura::client::DragDropClient,
      public ui::XEventDispatcher,
      public aura::WindowObserver,
      public X11MoveLoopDelegate {
 public:
  DesktopDragDropClientAuraX11(
      aura::Window* root_window,
      views::DesktopNativeCursorManager* cursor_manager,
      Display* xdisplay,
      XID xwindow);
  ~DesktopDragDropClientAuraX11() override;

  void Init();

  // aura::client::DragDropClient:
  int StartDragAndDrop(std::unique_ptr<ui::OSExchangeData> data,
                       aura::Window* root_window,
                       aura::Window* source_window,
                       const gfx::Point& screen_location,
                       int operation,
                       ui::DragDropTypes::DragEventSource source) override;
  void DragCancel() override;
  bool IsDragDropInProgress() override;
  void AddObserver(aura::client::DragDropClientObserver* observer) override;
  void RemoveObserver(aura::client::DragDropClientObserver* observer) override;

  // XEventDispatcher:
  bool DispatchXEvent(XEvent* event) override;

  // aura::WindowObserver:
  void OnWindowDestroyed(aura::Window* window) override;

  // X11MoveLoopDelegate:
  void OnMouseMovement(const gfx::Point& screen_point,
                       int flags,
                       base::TimeTicks event_time) override;
  void OnMouseReleased() override;
  void OnMoveLoopEnded() override;

 protected:
  // Getter for tests.
  Widget* drag_widget() { return drag_widget_.get(); }

  // Creates a move loop.  Virtual for testing.
  virtual std::unique_ptr<X11MoveLoop> CreateMoveLoop(
      X11MoveLoopDelegate* delegate);

 private:
  // When we receive a position X11 message, we need to translate that into
  // the underlying aura::Window representation, as moves internal to the X11
  // window can cause internal drag leave and enter messages.
  void DragTranslate(const gfx::Point& root_window_location,
                     std::unique_ptr<ui::OSExchangeData>* data,
                     std::unique_ptr<ui::DropTargetEvent>* event,
                     aura::client::DragDropDelegate** delegate);

  // Notifies |target_window_|'s drag delegate that we're no longer dragging,
  // then unsubscribes |target_window_| from ourselves and forgets it.
  void NotifyDragLeave();

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

  // A nested run loop that notifies this object of events through the
  // X11MoveLoopDelegate interface.
  std::unique_ptr<X11MoveLoop> move_loop_;

  aura::Window* root_window_;

  DesktopNativeCursorManager* cursor_manager_;

  // Events that we have selected on |source_window_|.
  std::unique_ptr<ui::XScopedEventSelector> source_window_events_;

  // The Aura window that is currently under the cursor. We need to manually
  // keep track of this because Windows will only call our drag enter method
  // once when the user enters the associated X Window. But inside that X
  // Window there could be multiple aura windows, so we need to generate drag
  // enter events for them.
  aura::Window* target_window_ = nullptr;

  // Because Xdnd messages don't contain the position in messages other than
  // the XdndPosition message, we must manually keep track of the last position
  // change.
  gfx::Point target_window_location_;
  gfx::Point target_window_root_location_;

  // The current drag-drop client that has an active operation. Since we have
  // multiple root windows and multiple DesktopDragDropClientAuraX11 instances
  // it is important to maintain only one drag and drop operation at any time.
  static DesktopDragDropClientAuraX11* g_current_drag_drop_client;

  // Widget that the user drags around.  May be nullptr.
  std::unique_ptr<Widget> drag_widget_;

  // The size of drag image.
  gfx::Size drag_image_size_;

  // The offset of |drag_widget_| relative to the mouse position.
  gfx::Vector2d drag_widget_offset_;

  base::WeakPtrFactory<DesktopDragDropClientAuraX11> weak_ptr_factory_{this};

  DISALLOW_COPY_AND_ASSIGN(DesktopDragDropClientAuraX11);
};

}  // namespace views

#endif  // UI_VIEWS_WIDGET_DESKTOP_AURA_DESKTOP_DRAG_DROP_CLIENT_AURAX11_H_
