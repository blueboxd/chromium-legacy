// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_BASE_X_X11_DRAG_DROP_CLIENT_H_
#define UI_BASE_X_X11_DRAG_DROP_CLIENT_H_

#include <vector>

#include "base/component_export.h"
#include "ui/base/dragdrop/drag_drop_types.h"
#include "ui/base/x/selection_utils.h"
#include "ui/base/x/x11_drag_context.h"
#include "ui/base/x/x11_topmost_window_finder.h"
#include "ui/gfx/geometry/point.h"
#include "ui/gfx/x/x11.h"

namespace ui {

// Converts a bitfield of actions into an Atom that represents what action
// we're most likely to take on drop.
COMPONENT_EXPORT(UI_BASE_X) Atom DragOperationToAtom(int drag_operation);

// Converts a single action atom to a drag operation.
COMPONENT_EXPORT(UI_BASE_X)
DragDropTypes::DragOperation AtomToDragOperation(Atom atom);

class COMPONENT_EXPORT(UI_BASE_X) XDragDropClient {
 public:
  // Handling XdndPosition can be paused while waiting for more data; this is
  // called either synchronously from OnXdndPosition, or asynchronously after
  // we've received data requested from the other window.
  virtual void CompleteXdndPosition(XID source_window,
                                    const gfx::Point& screen_point) = 0;

  int current_modifier_state() const { return current_modifier_state_; }

  // During the blocking StartDragAndDrop() call, this converts the views-style
  // |drag_operation_| bitfield into a vector of Atoms to offer to other
  // processes.
  std::vector<Atom> GetOfferedDragOperations() const;

  virtual void OnXdndEnter(const XClientMessageEvent& event);
  virtual void OnXdndPosition(const XClientMessageEvent& event);
  virtual void OnXdndStatus(const XClientMessageEvent& event);
  virtual void OnXdndLeave(const XClientMessageEvent& event);
  virtual void OnXdndDrop(const XClientMessageEvent& event);
  virtual void OnXdndFinished(const XClientMessageEvent& event);

 protected:
  XDragDropClient(Display* xdisplay, XID xwindow);
  virtual ~XDragDropClient();

  // We maintain a mapping of live XDragDropClient objects to their X11 windows,
  // so that we'd able to short circuit sending X11 messages to windows in our
  // process.
  static XDragDropClient* GetForWindow(XID window);

  XDragDropClient(const XDragDropClient&) = delete;
  XDragDropClient& operator=(const XDragDropClient&) = delete;

  Display* xdisplay() const { return xdisplay_; }
  XID xwindow() const { return xwindow_; }

  XID source_current_window() { return source_current_window_; }
  void set_source_current_window(XID window) {
    source_current_window_ = window;
  }

  XDragContext* target_current_context() {
    return target_current_context_.get();
  }
  void set_target_current_context(std::unique_ptr<XDragContext> context) {
    target_current_context_ = std::move(context);
  }

  // Resets the drag context.  Overrides should call this implementation.
  virtual void ResetDragContext();

  // Creates an XEvent and fills it in with values typical for XDND messages:
  // the type of event is set to ClientMessage, the format is set to 32 (longs),
  // and the zero member of data payload is set to |xwindow_|.  All other data
  // members are zeroed, as per XDND specification.
  XEvent PrepareXdndClientMessage(const char* message, XID recipient) const;

  // Finds the topmost X11 window at |screen_point| and returns it if it is
  // Xdnd aware.  Returns x11::None otherwise.
  // Virtual for testing.
  virtual XID FindWindowFor(const gfx::Point& screen_point);

  virtual std::unique_ptr<XTopmostWindowFinder> CreateWindowFinder() = 0;

  // Sends |xev| to |xid|, optionally short circuiting the round trip to the X
  // server.
  virtual void SendXClientEvent(XID xid, XEvent* xev);

  void SendXdndEnter(XID dest_window, const std::vector<Atom>& targets);
  void SendXdndLeave(XID dest_window);
  void SendXdndDrop(XID dest_window);

  virtual SelectionFormatMap GetFormatMap() const = 0;

  // The operation bitfield as requested by StartDragAndDrop.
  int drag_operation_ = 0;

  // The modifier state for the most recent mouse move.  Used to bypass an
  // asynchronous roundtrip through the X11 server.
  int current_modifier_state_ = 0;

 private:
  Display* const xdisplay_;
  const XID xwindow_;

  // Target side information.
  std::unique_ptr<XDragContext> target_current_context_;

  XID source_current_window_ = x11::None;
};

}  // namespace ui

#endif  // UI_BASE_X_X11_DRAG_DROP_CLIENT_H_
