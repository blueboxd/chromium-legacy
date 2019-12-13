// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/base/x/x11_drag_drop_client.h"

#include "ui/gfx/x/x11_atom_cache.h"

namespace ui {

namespace {
// These actions have the same meaning as in the W3C Drag and Drop spec.
const char kXdndActionCopy[] = "XdndActionCopy";
const char kXdndActionMove[] = "XdndActionMove";
const char kXdndActionLink[] = "XdndActionLink";
}  // namespace

Atom DragOperationToAtom(int drag_operation) {
  if (drag_operation & ui::DragDropTypes::DRAG_COPY)
    return gfx::GetAtom(kXdndActionCopy);
  if (drag_operation & ui::DragDropTypes::DRAG_MOVE)
    return gfx::GetAtom(kXdndActionMove);
  if (drag_operation & ui::DragDropTypes::DRAG_LINK)
    return gfx::GetAtom(kXdndActionLink);

  return x11::None;
}

DragDropTypes::DragOperation AtomToDragOperation(Atom atom) {
  if (atom == gfx::GetAtom(kXdndActionCopy))
    return DragDropTypes::DRAG_COPY;
  if (atom == gfx::GetAtom(kXdndActionMove))
    return DragDropTypes::DRAG_MOVE;
  if (atom == gfx::GetAtom(kXdndActionLink))
    return DragDropTypes::DRAG_LINK;

  return DragDropTypes::DRAG_NONE;
}

XDragDropClient::XDragDropClient(Display* xdisplay, XID xwindow)
    : xdisplay_(xdisplay), xwindow_(xwindow) {}

XDragDropClient::~XDragDropClient() = default;

XEvent XDragDropClient::PrepareXdndClientMessage(const char* message,
                                                 XID recipient) const {
  XEvent xev;
  xev.xclient.type = ClientMessage;
  xev.xclient.message_type = gfx::GetAtom(message);
  xev.xclient.format = 32;
  xev.xclient.window = recipient;
  xev.xclient.data.l[0] = xwindow_;
  xev.xclient.data.l[1] = 0;
  xev.xclient.data.l[2] = 0;
  xev.xclient.data.l[3] = 0;
  xev.xclient.data.l[4] = 0;
  return xev;
}

std::vector<Atom> XDragDropClient::GetOfferedDragOperations() const {
  std::vector<Atom> operations;
  if (drag_operation_ & ui::DragDropTypes::DRAG_COPY)
    operations.push_back(gfx::GetAtom(kXdndActionCopy));
  if (drag_operation_ & ui::DragDropTypes::DRAG_MOVE)
    operations.push_back(gfx::GetAtom(kXdndActionMove));
  if (drag_operation_ & ui::DragDropTypes::DRAG_LINK)
    operations.push_back(gfx::GetAtom(kXdndActionLink));
  return operations;
}

}  // namespace ui
