// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_BASE_X_X11_DRAG_CONTEXT_H_
#define UI_BASE_X_X11_DRAG_CONTEXT_H_

#include <memory>
#include <vector>

#include "base/component_export.h"
#include "ui/base/x/selection_utils.h"
#include "ui/events/platform/x11/x11_event_source.h"
#include "ui/gfx/geometry/point.h"
#include "ui/gfx/x/x11.h"

namespace ui {

class XDragDropClient;

class COMPONENT_EXPORT(UI_BASE_X) XDragContext {
 public:
  XDragContext(XID local_window,
               const XClientMessageEvent& event,
               XDragDropClient* source_client,
               const SelectionFormatMap& data);
  ~XDragContext();

  XDragContext(const XDragContext&) = delete;
  XDragContext& operator=(const XDragContext&) = delete;

  XID source_window() const { return source_window_; }
  XDragDropClient* source_client() { return source_client_; }
  const SelectionFormatMap& fetched_targets() const { return fetched_targets_; }

  // When we receive an XdndPosition message, we need to have all the data
  // copied from the other window before we process the XdndPosition
  // message. If we have that data already, dispatch immediately. Otherwise,
  // delay dispatching until we do.
  void OnXdndPositionMessage(XDragDropClient* client,
                             Atom suggested_action,
                             XID source_window,
                             Time time_stamp,
                             const gfx::Point& screen_point);

  // Called when XSelection data has been copied to our process.
  void OnSelectionNotify(const XSelectionEvent& xselection);

  // Reads the kXdndActionList property from |source_window_| and copies it
  // into |actions_|.
  void ReadActions();

  // Creates a DragDropTypes::DragOperation representation of the current
  // action list.
  int GetDragOperation() const;

  bool DispatchXEvent(XEvent* event);

 private:
  // Called to request the next target from the source window. This is only
  // done on the first XdndPosition; after that, we cache the data offered by
  // the source window.
  void RequestNextTarget();

  // Masks the X11 atom |xdnd_operation|'s views representation onto
  // |drag_operation|.
  void MaskOperation(Atom xdnd_operation, int* drag_operation) const;

  // The XID of our chrome local aura window handling our events.
  XID local_window_;

  // The XID of the window that initiated the drag.
  XID source_window_;

  // The DesktopDragDropClientAuraX11 for |source_window_| if |source_window_|
  // belongs to a Chrome window.
  XDragDropClient* source_client_;

  // The client we inform once we're done with requesting data.
  XDragDropClient* drag_drop_client_ = nullptr;

  // Whether we're blocking the handling of an XdndPosition message by waiting
  // for |unfetched_targets_| to be fetched.
  bool waiting_to_handle_position_ = false;

  // Where the cursor is on screen.
  gfx::Point screen_point_;

  // The time stamp of the last XdndPosition event we received.  The XDND
  // specification mandates that we use this time stamp when querying the source
  // about the drag and drop data.
  Time position_time_stamp_;

  // A SelectionFormatMap of data that we have in our process.
  SelectionFormatMap fetched_targets_;

  // The names of various data types offered by the other window that we
  // haven't fetched and put in |fetched_targets_| yet.
  std::vector<Atom> unfetched_targets_;

  // XdndPosition messages have a suggested action. Qt applications exclusively
  // use this, instead of the XdndActionList which is backed by |actions_|.
  Atom suggested_action_ = x11::None;

  // Possible actions.
  std::vector<Atom> actions_;
};

}  // namespace ui

#endif  // UI_BASE_X_X11_DRAG_CONTEXT_H_
