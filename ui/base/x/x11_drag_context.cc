// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/base/x/x11_drag_context.h"

#include "base/memory/ref_counted_memory.h"
#include "ui/base/dragdrop/drag_drop_types.h"
#include "ui/base/x/x11_drag_drop_client.h"
#include "ui/base/x/x11_util.h"
#include "ui/events/platform/platform_event_source.h"
#include "ui/gfx/x/x11_atom_cache.h"

namespace ui {

namespace {

// Window property that holds the supported drag and drop data types.
// This property is set on the XDND source window when the drag and drop data
// can be converted to more than 3 types.
const char kXdndTypeList[] = "XdndTypeList";

// Selection used by the XDND protocol to transfer data between applications.
const char kXdndSelection[] = "XdndSelection";

// Window property that contains the possible actions that will be presented to
// the user when the drag and drop action is kXdndActionAsk.
const char kXdndActionList[] = "XdndActionList";

// These actions have the same meaning as in the W3C Drag and Drop spec.
const char kXdndActionCopy[] = "XdndActionCopy";
const char kXdndActionMove[] = "XdndActionMove";
const char kXdndActionLink[] = "XdndActionLink";

// Window property that will receive the drag and drop selection data.
const char kChromiumDragReciever[] = "_CHROMIUM_DRAG_RECEIVER";

}  // namespace

XDragContext::XDragContext(XID local_window,
                           const XClientMessageEvent& event,
                           XDragDropClient* source_client,
                           const SelectionFormatMap& data)
    : local_window_(local_window),
      source_window_(event.data.l[0]),
      source_client_(source_client) {
  if (!source_client_) {
    bool get_types_from_property = ((event.data.l[1] & 1) != 0);

    if (get_types_from_property) {
      if (!GetAtomArrayProperty(source_window_, kXdndTypeList,
                                &unfetched_targets_)) {
        return;
      }
    } else {
      // data.l[2,3,4] contain the first three types. Unused slots can be None.
      for (size_t i = 2; i < 5; ++i) {
        if (event.data.l[i] != x11::None)
          unfetched_targets_.push_back(static_cast<x11::Atom>(event.data.l[i]));
      }
    }

#if DCHECK_IS_ON()
    DVLOG(1) << "XdndEnter has " << unfetched_targets_.size() << " data types";
    for (x11::Atom target : unfetched_targets_)
      DVLOG(1) << "XdndEnter data type: " << static_cast<uint32_t>(target);
#endif  // DCHECK_IS_ON()

    // We must perform a full sync here because we could be racing
    // |source_window_|.
    XSync(gfx::GetXDisplay(), x11::False);
  } else {
    // This drag originates from an aura window within our process. This means
    // that we can shortcut the X11 server and ask the owning SelectionOwner
    // for the data it's offering.
    fetched_targets_ = data;
  }

  ReadActions();
}

XDragContext::~XDragContext() = default;

void XDragContext::OnXdndPositionMessage(XDragDropClient* client,
                                         x11::Atom suggested_action,
                                         XID source_window,
                                         Time time_stamp,
                                         const gfx::Point& screen_point) {
  DCHECK_EQ(source_window_, source_window);
  suggested_action_ = suggested_action;

  if (!unfetched_targets_.empty()) {
    // We have unfetched targets. That means we need to pause the handling of
    // the position message and ask the other window for its data.
    screen_point_ = screen_point;
    drag_drop_client_ = client;
    position_time_stamp_ = time_stamp;
    waiting_to_handle_position_ = true;

    fetched_targets_ = SelectionFormatMap();
    RequestNextTarget();
  } else {
    client->CompleteXdndPosition(source_window, screen_point);
  }
}

void XDragContext::RequestNextTarget() {
  DCHECK(!unfetched_targets_.empty());
  DCHECK(drag_drop_client_);
  DCHECK(waiting_to_handle_position_);

  x11::Atom target = unfetched_targets_.back();
  unfetched_targets_.pop_back();

  XConvertSelection(gfx::GetXDisplay(),
                    static_cast<uint32_t>(gfx::GetAtom(kXdndSelection)),
                    static_cast<uint32_t>(target),
                    static_cast<uint32_t>(gfx::GetAtom(kChromiumDragReciever)),
                    local_window_, position_time_stamp_);
}

void XDragContext::OnSelectionNotify(const XSelectionEvent& event) {
  if (!waiting_to_handle_position_) {
    // A misbehaved window may send SelectionNotify without us requesting data
    // via XConvertSelection().
    return;
  }
  DCHECK(drag_drop_client_);

  DVLOG(1) << "SelectionNotify, format " << event.target;

  auto property = static_cast<x11::Atom>(event.property);
  auto target = static_cast<x11::Atom>(event.target);

  if (event.property != x11::None) {
    DCHECK_EQ(property, gfx::GetAtom(kChromiumDragReciever));

    std::vector<uint8_t> data;
    x11::Atom type = x11::Atom::None;
    if (GetRawBytesOfProperty(local_window_, property, &data, &type))
      fetched_targets_.Insert(target, base::RefCountedBytes::TakeVector(&data));
  } else {
    // The source failed to convert the drop data to the format (target in X11
    // parlance) that we asked for. This happens, even though we only ask for
    // the formats advertised by the source. http://crbug.com/628099
    LOG(ERROR) << "XConvertSelection failed for source-advertised target "
               << event.target;
  }

  if (!unfetched_targets_.empty()) {
    RequestNextTarget();
  } else {
    waiting_to_handle_position_ = false;
    drag_drop_client_->CompleteXdndPosition(source_window_, screen_point_);
    drag_drop_client_ = nullptr;
  }
}

void XDragContext::ReadActions() {
  if (!source_client_) {
    std::vector<x11::Atom> atom_array;
    if (!GetAtomArrayProperty(source_window_, kXdndActionList, &atom_array))
      actions_.clear();
    else
      actions_.swap(atom_array);
  } else {
    // We have a property notify set up for other windows in case they change
    // their action list. Thankfully, the views interface is static and you
    // can't change the action list after you enter StartDragAndDrop().
    actions_ = source_client_->GetOfferedDragOperations();
  }
}

int XDragContext::GetDragOperation() const {
  int drag_operation = DragDropTypes::DRAG_NONE;
  for (const auto& action : actions_)
    MaskOperation(action, &drag_operation);

  MaskOperation(suggested_action_, &drag_operation);

  return drag_operation;
}

void XDragContext::MaskOperation(x11::Atom xdnd_operation,
                                 int* drag_operation) const {
  if (xdnd_operation == gfx::GetAtom(kXdndActionCopy))
    *drag_operation |= DragDropTypes::DRAG_COPY;
  else if (xdnd_operation == gfx::GetAtom(kXdndActionMove))
    *drag_operation |= DragDropTypes::DRAG_MOVE;
  else if (xdnd_operation == gfx::GetAtom(kXdndActionLink))
    *drag_operation |= DragDropTypes::DRAG_LINK;
}

bool XDragContext::DispatchXEvent(XEvent* xev) {
  if (xev->type == PropertyNotify &&
      static_cast<x11::Atom>(xev->xproperty.atom) ==
          gfx::GetAtom(kXdndActionList)) {
    ReadActions();
    return true;
  }
  return false;
}

}  // namespace ui
