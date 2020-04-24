// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_BASE_X_X11_DRAG_DROP_CLIENT_H_
#define UI_BASE_X_X11_DRAG_DROP_CLIENT_H_

#include <vector>

#include "base/component_export.h"
#include "base/timer/timer.h"
#include "ui/base/dragdrop/drag_drop_types.h"
#include "ui/base/x/selection_utils.h"
#include "ui/base/x/x11_drag_context.h"
#include "ui/base/x/x11_topmost_window_finder.h"
#include "ui/gfx/geometry/point.h"
#include "ui/gfx/x/x11.h"

namespace ui {

class OSExchangeData;
class XOSExchangeDataProvider;

// Converts the current set of X masks into the set of ui::EventFlags.
COMPONENT_EXPORT(UI_BASE_X) int XGetMaskAsEventFlags();

// Handles XDND (X11 drag and drop protocol) for the given window.
//
// Doesn't fetch XDND events from the event source; those should be taken by
// the client and fed to |OnXdnd...| and |OnSelectionNotify| methods.
class COMPONENT_EXPORT(UI_BASE_X) XDragDropClient {
 public:
  // Handlers and callbacks that should be implemented at the consumer side.
  class Delegate {
   public:
    // Creates the window finder.
    virtual std::unique_ptr<XTopmostWindowFinder> CreateWindowFinder() = 0;

    // Updates the drag status by the new position.  Returns the drag operation
    // possible at that position.
    //
    // Handling XdndPosition can be paused while waiting for more data; this is
    // called either synchronously from OnXdndPosition, or asynchronously after
    // we've received data requested from the other window.
    virtual int UpdateDrag(const gfx::Point& screen_point) = 0;

    // Updates the mouse cursor shape.
    virtual void UpdateCursor(
        DragDropTypes::DragOperation negotiated_operation) = 0;

    // Called when data from another application enters the window.
    virtual void OnBeginForeignDrag(XID window) = 0;

    // Called when data from another application is about to leave the window.
    virtual void OnEndForeignDrag() = 0;

    // Called just before the drag leaves the window.
    virtual void OnBeforeDragLeave() = 0;

    // Drops data at the current location and returns the resulting operation.
    virtual int PerformDrop() = 0;

    // Called to end the move loop that is maintained by the subclass.
    virtual void EndMoveLoop() = 0;

   protected:
    virtual ~Delegate() {}
  };

  // Handling XdndPosition can be paused while waiting for more data; this is
  // called either synchronously from OnXdndPosition, or asynchronously after
  // we've received data requested from the other window.
  void CompleteXdndPosition(XID source_window, const gfx::Point& screen_point);

  void ProcessMouseMove(const gfx::Point& screen_point,
                        unsigned long event_time);

  const XOSExchangeDataProvider* source_provider() const {
    return source_provider_;
  }
  int current_modifier_state() const { return current_modifier_state_; }

  // During the blocking StartDragAndDrop() call, this converts the views-style
  // |drag_operation_| bitfield into a vector of Atoms to offer to other
  // processes.
  std::vector<Atom> GetOfferedDragOperations() const;

  // These methods handle the various X11 client messages from the platform.
  void OnXdndEnter(const XClientMessageEvent& event);
  void OnXdndPosition(const XClientMessageEvent& event);
  void OnXdndStatus(const XClientMessageEvent& event);
  void OnXdndLeave(const XClientMessageEvent& event);
  void OnXdndDrop(const XClientMessageEvent& event);
  void OnXdndFinished(const XClientMessageEvent& event);

  // Called when XSelection data has been copied to our process.
  void OnSelectionNotify(const XSelectionEvent& xselection);

 protected:
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

  XDragDropClient(Delegate* delegate, Display* xdisplay, XID xwindow);
  virtual ~XDragDropClient();

  XDragDropClient(const XDragDropClient&) = delete;
  XDragDropClient& operator=(const XDragDropClient&) = delete;

  XID xwindow() const { return xwindow_; }
  XDragContext* target_current_context() {
    return target_current_context_.get();
  }
  DragDropTypes::DragOperation negotiated_operation() const {
    return negotiated_operation_;
  }

  // Resets the drag state so the object is ready to handle the drag.  Sets
  // X window properties so that the desktop environment is aware of available
  // actions.  Sets |source_provider_| so properties of dragged data can be
  // queried afterwards.  Should be called before entering the main drag loop.
  void InitDrag(int operation, OSExchangeData* data);

  // Cleans up the drag state after the drag is done.  Removes the X window
  // properties related to the drag operation.  Should be called after exiting
  // the main drag loop.
  void CleanupDrag();

  // Updates |current_modifier_state_| with the given set of flags.
  void UpdateModifierState(int flags);

  // Resets the drag context.  Calls |OnEndForeignDrag()| if necessary.
  void ResetDragContext();

  void StopRepeatMouseMoveTimer();

  // Start timer to end the move loop if the target is too slow to respond after
  // the mouse is released.
  void StartEndMoveLoopTimer();
  void StopEndMoveLoopTimer();

  // These |Handle...| methods essentially implement the
  // views::X11MoveLoopDelegate interface.
  void HandleMouseMovement(const gfx::Point& screen_point,
                           int flags,
                           base::TimeTicks event_time);
  void HandleMouseReleased();
  void HandleMoveLoopEnded();

 private:
  // Creates an XEvent and fills it in with values typical for XDND messages:
  // the type of event is set to ClientMessage, the format is set to 32 (longs),
  // and the zero member of data payload is set to |xwindow_|.  All other data
  // members are zeroed, as per XDND specification.
  XEvent PrepareXdndClientMessage(const char* message, XID recipient) const;

  // Finds the topmost X11 window at |screen_point| and returns it if it is
  // Xdnd aware.  Returns x11::None otherwise.
  // Virtual for testing.
  virtual XID FindWindowFor(const gfx::Point& screen_point);

  // Sends |xev| to |xid|, optionally short circuiting the round trip to the X
  // server.
  // Virtual for testing.
  virtual void SendXClientEvent(XID xid, XEvent* xev);

  void SendXdndEnter(XID dest_window, const std::vector<Atom>& targets);
  void SendXdndPosition(XID dest_window,
                        const gfx::Point& screen_point,
                        unsigned long event_time);
  void SendXdndLeave(XID dest_window);
  void SendXdndDrop(XID dest_window);

  // We maintain a mapping of live XDragDropClient objects to their X11 windows,
  // so that we'd able to short circuit sending X11 messages to windows in our
  // process.
  static XDragDropClient* GetForWindow(XID window);

  void EndMoveLoop();

  Delegate* const delegate_;

  Display* const xdisplay_;
  const XID xwindow_;

  // Target side information.
  std::unique_ptr<XDragContext> target_current_context_;

  // Source side information.
  XID source_current_window_ = x11::None;
  SourceState source_state_ = SourceState::kOther;
  const XOSExchangeDataProvider* source_provider_ = nullptr;

  // The operation bitfield as requested by StartDragAndDrop.
  int drag_operation_ = 0;

  // The modifier state for the most recent mouse move.  Used to bypass an
  // asynchronous roundtrip through the X11 server.
  int current_modifier_state_ = 0;

  // We offer the other window a list of possible operations,
  // XdndActionsList. This is the requested action from the other window. This
  // is DRAG_NONE if we haven't sent out an XdndPosition message yet, haven't
  // yet received an XdndStatus or if the other window has told us that there's
  // no action that we can agree on.
  DragDropTypes::DragOperation negotiated_operation_ = DragDropTypes::DRAG_NONE;

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

  // Ends the move loop if the target is too slow to respond after the mouse is
  // released.
  base::OneShotTimer end_move_loop_timer_;

  // When the mouse is released, we need to wait for the last XdndStatus message
  // only if we have previously received a status message from
  // |source_current_window_|.
  bool status_received_since_enter_ = false;
};

}  // namespace ui

#endif  // UI_BASE_X_X11_DRAG_DROP_CLIENT_H_
