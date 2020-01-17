// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_VIEWS_WIDGET_DESKTOP_AURA_DESKTOP_DRAG_DROP_CLIENT_AURAX11_H_
#define UI_VIEWS_WIDGET_DESKTOP_AURA_DESKTOP_DRAG_DROP_CLIENT_AURAX11_H_

#include <memory>
#include <set>
#include <vector>

#include "base/compiler_specific.h"
#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "base/timer/timer.h"
#include "ui/aura/client/drag_drop_client.h"
#include "ui/aura/window_observer.h"
#include "ui/base/cursor/cursor.h"
#include "ui/base/dragdrop/drag_drop_types.h"
#include "ui/base/x/x11_drag_drop_client.h"
#include "ui/events/event_constants.h"
#include "ui/events/platform/platform_event_dispatcher.h"
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
}
}

namespace gfx {
class ImageSkia;
class Point;
}

namespace ui {
class DropTargetEvent;
class OSExchangeData;
class OSExchangeDataProviderAuraX11;
class XTopmostWindowFinder;
}

namespace views {
class DesktopNativeCursorManager;
class Widget;
class X11MoveLoop;

// Implements drag and drop on X11 for aura. On one side, this class takes raw
// X11 events forwarded from DesktopWindowTreeHostLinux, while on the other, it
// handles the views drag events.
class VIEWS_EXPORT DesktopDragDropClientAuraX11
    : public ui::XDragDropClient,
      public aura::client::DragDropClient,
      public ui::PlatformEventDispatcher,
      public aura::WindowObserver,
      public X11MoveLoopDelegate {
 public:
  DesktopDragDropClientAuraX11(
      aura::Window* root_window,
      views::DesktopNativeCursorManager* cursor_manager,
      ::Display* xdisplay,
      ::Window xwindow);
  ~DesktopDragDropClientAuraX11() override;

  void Init();

  // These methods handle the various X11 client messages from the platform.
  void OnXdndEnter(const XClientMessageEvent& event) override;
  void OnXdndLeave(const XClientMessageEvent& event) override;
  void OnXdndStatus(const XClientMessageEvent& event) override;
  void OnXdndFinished(const XClientMessageEvent& event) override;
  void OnXdndDrop(const XClientMessageEvent& event) override;

  // Called when XSelection data has been copied to our process.
  void OnSelectionNotify(const XSelectionEvent& xselection);

  // Overridden from aura::client::DragDropClient:
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

  // PlatformEventDispatcher:
  bool CanDispatchEvent(const ui::PlatformEvent& event) override;
  uint32_t DispatchEvent(const ui::PlatformEvent& event) override;

  // Overridden from aura::WindowObserver:
  void OnWindowDestroyed(aura::Window* window) override;

  // Overridden from X11WholeScreenMoveLoopDelegate:
  void OnMouseMovement(const gfx::Point& screen_point,
                       int flags,
                       base::TimeTicks event_time) override;
  void OnMouseReleased() override;
  void OnMoveLoopEnded() override;

 protected:
  // The following methods are virtual for the sake of testing.

  // Creates a move loop.
  virtual std::unique_ptr<X11MoveLoop> CreateMoveLoop(
      X11MoveLoopDelegate* delegate);

 protected:
  Widget* drag_widget() { return drag_widget_.get(); }

 private:
  enum class SourceState {
    // |source_current_window_| will receive a drop once we receive an
    // XdndStatus from it.
    kPendingDrop,

    // The move looped will be ended once we receive XdndFinished from
    // |source_current_window_|. We should not send XdndPosition to
    // |source_current_window_| while in this state.
    kDropped,

    // There is no drag in progress or there is a drag in progress and the
    // user has not yet released the mouse.
    kOther,
  };

  // Processes a mouse move at |screen_point|.
  void ProcessMouseMove(const gfx::Point& screen_point,
                        unsigned long event_time);

  // Start timer to end the move loop if the target is too slow to respond after
  // the mouse is released.
  void StartEndMoveLoopTimer();

  // Ends the move loop.
  void EndMoveLoop();

  // When we receive an position x11 message, we need to translate that into
  // the underlying aura::Window representation, as moves internal to the X11
  // window can cause internal drag leave and enter messages.
  void DragTranslate(const gfx::Point& root_window_location,
                     std::unique_ptr<ui::OSExchangeData>* data,
                     std::unique_ptr<ui::DropTargetEvent>* event,
                     aura::client::DragDropDelegate** delegate);

  // Called when we need to notify the current aura::Window that we're no
  // longer dragging over it.
  void NotifyDragLeave();

  // This returns a representation of the data we're offering in this
  // drag. This is done to bypass an asynchronous roundtrip with the X11
  // server.
  ui::SelectionFormatMap GetFormatMap() const override;

  // Handling XdndPosition can be paused while waiting for more data; this is
  // called either synchronously from OnXdndPosition, or asynchronously after
  // we've received data requested from the other window.
  void CompleteXdndPosition(::Window source_window,
                            const gfx::Point& screen_point) override;

  void SendXdndPosition(::Window dest_window,
                        const gfx::Point& screen_point,
                        unsigned long event_time);

  // Creates a widget for the user to drag around.
  void CreateDragWidget(const gfx::ImageSkia& image);

  // Returns true if |image| has any visible regions (defined as having a pixel
  // with alpha > 32).
  bool IsValidDragImage(const gfx::ImageSkia& image);

  void ResetDragContext() override;

  std::unique_ptr<ui::XTopmostWindowFinder> CreateWindowFinder() override;

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

  // In the Xdnd protocol, we aren't supposed to send another XdndPosition
  // message until we have received a confirming XdndStatus message.
  bool waiting_on_status_ = false;

  // If we would send an XdndPosition message while we're waiting for an
  // XdndStatus response, we need to cache the latest details we'd send.
  std::unique_ptr<std::pair<gfx::Point, unsigned long>> next_position_message_;

  // Reprocesses the most recent mouse move event if the mouse has not moved
  // in a while in case the window stacking order has changed and
  // |source_current_window_| needs to be updated.
  base::OneShotTimer repeat_mouse_move_timer_;

  // When the mouse is released, we need to wait for the last XdndStatus message
  // only if we have previously received a status message from
  // |source_current_window_|.
  bool status_received_since_enter_ = false;

  // Source side information.
  ui::OSExchangeDataProviderAuraX11 const* source_provider_ = nullptr;
  SourceState source_state_ = SourceState::kOther;

  // The current drag-drop client that has an active operation. Since we have
  // multiple root windows and multiple DesktopDragDropClientAuraX11 instances
  // it is important to maintain only one drag and drop operation at any time.
  static DesktopDragDropClientAuraX11* g_current_drag_drop_client;

  // We offer the other window a list of possible operations,
  // XdndActionsList. This is the requested action from the other window. This
  // is DRAG_NONE if we haven't sent out an XdndPosition message yet, haven't
  // yet received an XdndStatus or if the other window has told us that there's
  // no action that we can agree on.
  ui::DragDropTypes::DragOperation negotiated_operation_ =
      ui::DragDropTypes::DRAG_NONE;

  // Ends the move loop if the target is too slow to respond after the mouse is
  // released.
  base::OneShotTimer end_move_loop_timer_;

  // Widget that the user drags around. May be NULL.
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
