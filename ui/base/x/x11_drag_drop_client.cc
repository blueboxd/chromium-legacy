// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/base/x/x11_drag_drop_client.h"

#include "base/lazy_instance.h"
#include "ui/base/clipboard/clipboard_constants.h"
#include "ui/base/x/x11_os_exchange_data_provider.h"
#include "ui/base/x/x11_util.h"
#include "ui/gfx/x/x11_atom_cache.h"

// Reading recommended for understanding the implementation in this file:
//
// * The X Window System Concepts section in The X New Developer’s Guide
// * The X Selection Mechanism paper by Keith Packard
// * The Peer-to-Peer Communication by Means of Selections section in the
//   ICCCM (X Consortium's Inter-Client Communication Conventions Manual)
// * The XDND specification, Drag-and-Drop Protocol for the X Window System
// * The XDS specification, The Direct Save Protocol for the X Window System
//
// All the readings are freely available online.

namespace ui {

namespace {

constexpr int kWillAcceptDrop = 1;
constexpr int kWantFurtherPosEvents = 2;

// The lowest XDND protocol version that we understand.
//
// The XDND protocol specification says that we must support all versions
// between 3 and the version we advertise in the XDndAware property.
constexpr int kMinXdndVersion = 3;

// The value used in the XdndAware property.
//
// The XDND protocol version used between two windows will be the minimum
// between the two versions advertised in the XDndAware property.
constexpr int kMaxXdndVersion = 5;

// Window property that tells other applications the window understands XDND.
const char kXdndAware[] = "XdndAware";

// Window property that holds the supported drag and drop data types.
// This property is set on the XDND source window when the drag and drop data
// can be converted to more than 3 types.
const char kXdndTypeList[] = "XdndTypeList";

// These actions have the same meaning as in the W3C Drag and Drop spec.
const char kXdndActionCopy[] = "XdndActionCopy";
const char kXdndActionMove[] = "XdndActionMove";
const char kXdndActionLink[] = "XdndActionLink";

// Triggers the XDS protocol.
const char kXdndActionDirectSave[] = "XdndActionDirectSave";

// Window property that contains the possible actions that will be presented to
// the user when the drag and drop action is kXdndActionAsk.
const char kXdndActionList[] = "XdndActionList";

// Window property on the source window and message used by the XDS protocol.
// This atom name intentionally includes the XDS protocol version (0).
// After the source sends the XdndDrop message, this property stores the
// (path-less) name of the file to be saved, and has the type text/plain, with
// an optional charset attribute.
// When receiving an XdndDrop event, the target needs to check for the
// XdndDirectSave property on the source window. The target then modifies the
// XdndDirectSave on the source window, and sends an XdndDirectSave message to
// the source.
// After the target sends the XdndDirectSave message, this property stores an
// URL indicating the location where the source should save the file.
const char kXdndDirectSave0[] = "XdndDirectSave0";

// Window property pointing to a proxy window to receive XDND target messages.
// The XDND source must check the proxy window must for the XdndAware property,
// and must send all XDND messages to the proxy instead of the target. However,
// the target field in the messages must still represent the original target
// window (the window pointed to by the cursor).
const char kXdndProxy[] = "XdndProxy";

// Message sent from an XDND source to the target when the user confirms the
// drag and drop operation.
const char kXdndDrop[] = "XdndDrop";

// Message sent from an XDND source to the target to start the XDND protocol.
// The target must wait for an XDndPosition event before querying the data.
const char kXdndEnter[] = "XdndEnter";

// Message sent from an XDND target to the source in response to an XdndDrop.
// The message must be sent whether the target acceepts the drop or not.
const char kXdndFinished[] = "XdndFinished";

// Message sent from an XDND source to the target when the user cancels the drag
// and drop operation.
const char kXdndLeave[] = "XdndLeave";

// Message sent by the XDND source when the cursor position changes.
// The source will also send an XdndPosition event right after the XdndEnter
// event, to tell the target about the initial cursor position and the desired
// drop action.
// The time stamp in the XdndPosition must be used when requesting selection
// information.
// After the target optionally acquires selection information, it must tell the
// source if it can accept the drop via an XdndStatus message.
const char kXdndPosition[] = "XdndPosition";

// Message sent by the XDND target in response to an XdndPosition message.
// The message informs the source if the target will accept the drop, and what
// action will be taken if the drop is accepted.
const char kXdndStatus[] = "XdndStatus";

static base::LazyInstance<std::map<XID, XDragDropClient*>>::Leaky
    g_live_client_map = LAZY_INSTANCE_INITIALIZER;

// Converts a bitfield of actions into an Atom that represents what action
// we're most likely to take on drop.
Atom XDragOperationToAtom(int drag_operation) {
  if (drag_operation & DragDropTypes::DRAG_COPY)
    return gfx::GetAtom(kXdndActionCopy);
  if (drag_operation & DragDropTypes::DRAG_MOVE)
    return gfx::GetAtom(kXdndActionMove);
  if (drag_operation & DragDropTypes::DRAG_LINK)
    return gfx::GetAtom(kXdndActionLink);

  return x11::None;
}

// Converts a single action atom to a drag operation.
DragDropTypes::DragOperation XAtomToDragOperation(Atom atom) {
  if (atom == gfx::GetAtom(kXdndActionCopy))
    return DragDropTypes::DRAG_COPY;
  if (atom == gfx::GetAtom(kXdndActionMove))
    return DragDropTypes::DRAG_MOVE;
  if (atom == gfx::GetAtom(kXdndActionLink))
    return DragDropTypes::DRAG_LINK;

  return DragDropTypes::DRAG_NONE;
}

}  // namespace

int XGetMaskAsEventFlags() {
  XDisplay* display = gfx::GetXDisplay();

  XID root, child;
  int root_x, root_y, win_x, win_y;
  unsigned int mask;
  XQueryPointer(display, DefaultRootWindow(display), &root, &child, &root_x,
                &root_y, &win_x, &win_y, &mask);
  int modifiers = ui::EF_NONE;
  if (mask & ShiftMask)
    modifiers |= ui::EF_SHIFT_DOWN;
  if (mask & ControlMask)
    modifiers |= ui::EF_CONTROL_DOWN;
  if (mask & Mod1Mask)
    modifiers |= ui::EF_ALT_DOWN;
  if (mask & Mod4Mask)
    modifiers |= ui::EF_COMMAND_DOWN;
  if (mask & Button1Mask)
    modifiers |= ui::EF_LEFT_MOUSE_BUTTON;
  if (mask & Button2Mask)
    modifiers |= ui::EF_MIDDLE_MOUSE_BUTTON;
  if (mask & Button3Mask)
    modifiers |= ui::EF_RIGHT_MOUSE_BUTTON;
  return modifiers;
}

// static
XDragDropClient* XDragDropClient::GetForWindow(XID window) {
  std::map<XID, XDragDropClient*>::const_iterator it =
      g_live_client_map.Get().find(window);
  if (it == g_live_client_map.Get().end())
    return nullptr;
  return it->second;
}

XDragDropClient::XDragDropClient(XDragDropClient::Delegate* delegate,
                                 Display* xdisplay,
                                 XID xwindow)
    : delegate_(delegate), xdisplay_(xdisplay), xwindow_(xwindow) {
  DCHECK(delegate_);

  // Mark that we are aware of drag and drop concepts.
  unsigned long xdnd_version = kMaxXdndVersion;
  XChangeProperty(xdisplay_, xwindow_, gfx::GetAtom(kXdndAware), XA_ATOM, 32,
                  PropModeReplace,
                  reinterpret_cast<unsigned char*>(&xdnd_version), 1);

  // Some tests change the DesktopDragDropClientAuraX11 associated with an
  // |xwindow|.
  g_live_client_map.Get()[xwindow] = this;
}

XDragDropClient::~XDragDropClient() {
  g_live_client_map.Get().erase(xwindow());
}

std::vector<Atom> XDragDropClient::GetOfferedDragOperations() const {
  std::vector<Atom> operations;
  if (drag_operation_ & DragDropTypes::DRAG_COPY)
    operations.push_back(gfx::GetAtom(kXdndActionCopy));
  if (drag_operation_ & DragDropTypes::DRAG_MOVE)
    operations.push_back(gfx::GetAtom(kXdndActionMove));
  if (drag_operation_ & DragDropTypes::DRAG_LINK)
    operations.push_back(gfx::GetAtom(kXdndActionLink));
  return operations;
}

void XDragDropClient::CompleteXdndPosition(XID source_window,
                                           const gfx::Point& screen_point) {
  int drag_operation = delegate_->UpdateDrag(screen_point);

  // Sends an XdndStatus message back to the source_window. l[2,3]
  // theoretically represent an area in the window where the current action is
  // the same as what we're returning, but I can't find any implementation that
  // actually making use of this. A client can return (0, 0) and/or set the
  // first bit of l[1] to disable the feature, and it appears that gtk neither
  // sets this nor respects it if set.
  XEvent xev = PrepareXdndClientMessage(kXdndStatus, source_window);
  xev.xclient.data.l[1] =
      (drag_operation != 0) ? (kWantFurtherPosEvents | kWillAcceptDrop) : 0;
  xev.xclient.data.l[4] = XDragOperationToAtom(drag_operation);
  SendXClientEvent(source_window, &xev);
}

void XDragDropClient::ProcessMouseMove(const gfx::Point& screen_point,
                                       unsigned long event_time) {
  if (source_state_ != SourceState::kOther)
    return;

  // Find the current window the cursor is over.
  XID dest_window = FindWindowFor(screen_point);

  if (source_current_window_ != dest_window) {
    if (source_current_window_ != x11::None)
      SendXdndLeave(source_current_window_);

    source_current_window_ = dest_window;
    waiting_on_status_ = false;
    next_position_message_.reset();
    status_received_since_enter_ = false;
    negotiated_operation_ = DragDropTypes::DRAG_NONE;

    if (source_current_window_ != x11::None) {
      std::vector<Atom> targets;
      source_provider_->RetrieveTargets(&targets);
      SendXdndEnter(source_current_window_, targets);
    }
  }

  if (source_current_window_ != x11::None) {
    if (waiting_on_status_) {
      next_position_message_ =
          std::make_unique<std::pair<gfx::Point, unsigned long>>(screen_point,
                                                                 event_time);
    } else {
      SendXdndPosition(dest_window, screen_point, event_time);
    }
  }
}

void XDragDropClient::OnXdndEnter(const XClientMessageEvent& event) {
  DVLOG(1) << "OnXdndEnter, version " << ((event.data.l[1] & 0xff000000) >> 24);

  int version = (event.data.l[1] & 0xff000000) >> 24;
  if (version < kMinXdndVersion) {
    // This protocol version is not documented in the XDND standard (last
    // revised in 1999), so we don't support it. Since don't understand the
    // protocol spoken by the source, we can't tell it that we can't talk to it.
    LOG(ERROR) << "XdndEnter message discarded because its version is too old.";
    return;
  }
  if (version > kMaxXdndVersion) {
    // The XDND version used should be the minimum between the versions
    // advertised by the source and the target. We advertise kMaxXdndVersion, so
    // this should never happen when talking to an XDND-compliant application.
    LOG(ERROR) << "XdndEnter message discarded because its version is too new.";
    return;
  }

  // Make sure that we've run ~X11DragContext() before creating another one.
  ResetDragContext();
  auto* source_client = GetForWindow(event.data.l[0]);
  DCHECK(!source_client || source_client->source_provider_);
  target_current_context_ = std::make_unique<XDragContext>(
      xwindow_, event, source_client,
      (source_client ? source_client->source_provider_->GetFormatMap()
                     : SelectionFormatMap()));

  if (!target_current_context()->source_client()) {
    // The window doesn't have a DesktopDragDropClientAuraX11, which means it's
    // created by some other process.  Listen for messages on it.
    delegate_->OnBeginForeignDrag(event.data.l[0]);
  }

  // In the Windows implementation, we immediately call DesktopDropTargetWin::
  // Translate().  The XDND specification demands that we wait until we receive
  // an XdndPosition message before we use XConvertSelection or send an
  // XdndStatus message.
}

void XDragDropClient::OnXdndPosition(const XClientMessageEvent& event) {
  DVLOG(1) << "OnXdndPosition";

  XID source_window = event.data.l[0];
  int x_root_window = event.data.l[2] >> 16;
  int y_root_window = event.data.l[2] & 0xffff;
  Time time_stamp = event.data.l[3];
  Atom suggested_action = event.data.l[4];

  if (!target_current_context()) {
    NOTREACHED();
    return;
  }

  target_current_context()->OnXdndPositionMessage(
      this, suggested_action, source_window, time_stamp,
      gfx::Point(x_root_window, y_root_window));
}

void XDragDropClient::OnXdndStatus(const XClientMessageEvent& event) {
  DVLOG(1) << "OnXdndStatus";

  XID source_window = event.data.l[0];

  if (source_window != source_current_window_)
    return;

  if (source_state_ != SourceState::kPendingDrop &&
      source_state_ != SourceState::kOther) {
    return;
  }

  waiting_on_status_ = false;
  status_received_since_enter_ = true;

  if (event.data.l[1] & 1) {
    Atom atom_operation = event.data.l[4];
    negotiated_operation_ = XAtomToDragOperation(atom_operation);
  } else {
    negotiated_operation_ = DragDropTypes::DRAG_NONE;
  }

  if (source_state_ == SourceState::kPendingDrop) {
    // We were waiting on the status message so we could send the XdndDrop.
    if (negotiated_operation_ == DragDropTypes::DRAG_NONE) {
      EndMoveLoop();
      return;
    }
    source_state_ = SourceState::kDropped;
    SendXdndDrop(source_window);
    return;
  }

  delegate_->UpdateCursor(negotiated_operation_);

  // Note: event.data.[2,3] specify a rectangle. It is a request by the other
  // window to not send further XdndPosition messages while the cursor is
  // within it. However, it is considered advisory and (at least according to
  // the spec) the other side must handle further position messages within
  // it. GTK+ doesn't bother with this, so neither should we.

  if (next_position_message_.get()) {
    // We were waiting on the status message so we could send off the next
    // position message we queued up.
    gfx::Point p = next_position_message_->first;
    unsigned long event_time = next_position_message_->second;
    next_position_message_.reset();

    SendXdndPosition(source_window, p, event_time);
  }
}

void XDragDropClient::OnXdndLeave(const XClientMessageEvent& event) {
  DVLOG(1) << "OnXdndLeave";
  delegate_->OnBeforeDragLeave();
  ResetDragContext();
}

void XDragDropClient::OnXdndDrop(const XClientMessageEvent& event) {
  DVLOG(1) << "OnXdndDrop";

  XID source_window = event.data.l[0];

  int drag_operation = delegate_->PerformDrop();

  XEvent xev = PrepareXdndClientMessage(kXdndFinished, source_window);
  xev.xclient.data.l[1] = (drag_operation != 0) ? 1 : 0;
  xev.xclient.data.l[2] = XDragOperationToAtom(drag_operation);
  SendXClientEvent(source_window, &xev);
}

void XDragDropClient::OnXdndFinished(const XClientMessageEvent& event) {
  DVLOG(1) << "OnXdndFinished";
  XID source_window = event.data.l[0];
  if (source_current_window_ != source_window)
    return;

  // Clear |negotiated_operation_| if the drag was rejected.
  if ((event.data.l[1] & 1) == 0)
    negotiated_operation_ = DragDropTypes::DRAG_NONE;

  // Clear |source_current_window_| to avoid sending XdndLeave upon ending the
  // move loop.
  source_current_window_ = x11::None;
  EndMoveLoop();
}

void XDragDropClient::OnSelectionNotify(const XSelectionEvent& xselection) {
  DVLOG(1) << "OnSelectionNotify";
  if (target_current_context_)
    target_current_context_->OnSelectionNotify(xselection);

  // ICCCM requires us to delete the property passed into SelectionNotify.
  if (xselection.property != x11::None)
    XDeleteProperty(xdisplay_, xwindow_, xselection.property);
}

void XDragDropClient::InitDrag(int operation, OSExchangeData* data) {
  source_current_window_ = x11::None;
  source_state_ = SourceState::kOther;
  waiting_on_status_ = false;
  next_position_message_.reset();
  status_received_since_enter_ = false;
  drag_operation_ = operation;
  negotiated_operation_ = DragDropTypes::DRAG_NONE;

  source_provider_ =
      static_cast<const XOSExchangeDataProvider*>(&data->provider());
  source_provider_->TakeOwnershipOfSelection();

  std::vector<Atom> actions = GetOfferedDragOperations();
  if (!source_provider_->file_contents_name().empty()) {
    actions.push_back(gfx::GetAtom(kXdndActionDirectSave));
    SetStringProperty(xwindow_, gfx::GetAtom(kXdndDirectSave0),
                      gfx::GetAtom(kMimeTypeText),
                      source_provider_->file_contents_name().AsUTF8Unsafe());
  }
  SetAtomArrayProperty(xwindow_, kXdndActionList, "ATOM", actions);
}

void XDragDropClient::CleanupDrag() {
  source_provider_ = nullptr;
  XDeleteProperty(xdisplay_, xwindow_, gfx::GetAtom(kXdndActionList));
  XDeleteProperty(xdisplay_, xwindow_, gfx::GetAtom(kXdndDirectSave0));
}

void XDragDropClient::UpdateModifierState(int flags) {
  const int kModifiers = EF_SHIFT_DOWN | EF_CONTROL_DOWN | EF_ALT_DOWN |
                         EF_COMMAND_DOWN | EF_LEFT_MOUSE_BUTTON |
                         EF_MIDDLE_MOUSE_BUTTON | EF_RIGHT_MOUSE_BUTTON;
  current_modifier_state_ = flags & kModifiers;
}

void XDragDropClient::ResetDragContext() {
  if (!target_current_context())
    return;
  if (!target_current_context()->source_client())
    delegate_->OnEndForeignDrag();

  target_current_context_.reset();
}

void XDragDropClient::StopRepeatMouseMoveTimer() {
  repeat_mouse_move_timer_.Stop();
}

void XDragDropClient::StartEndMoveLoopTimer() {
  end_move_loop_timer_.Start(FROM_HERE, base::TimeDelta::FromMilliseconds(1000),
                             this, &XDragDropClient::EndMoveLoop);
}

void XDragDropClient::StopEndMoveLoopTimer() {
  end_move_loop_timer_.Stop();
}

void XDragDropClient::HandleMouseMovement(const gfx::Point& screen_point,
                                          int flags,
                                          base::TimeTicks event_time) {
  UpdateModifierState(flags);
  StopRepeatMouseMoveTimer();
  ProcessMouseMove(screen_point,
                   (event_time - base::TimeTicks()).InMilliseconds());
}

void XDragDropClient::HandleMouseReleased() {
  StopRepeatMouseMoveTimer();

  if (source_state_ != SourceState::kOther) {
    // The user has previously released the mouse and is clicking in
    // frustration.
    EndMoveLoop();
    return;
  }

  if (source_current_window_ != x11::None) {
    if (waiting_on_status_) {
      if (status_received_since_enter_) {
        // If we are waiting for an XdndStatus message, we need to wait for it
        // to complete.
        source_state_ = SourceState::kPendingDrop;

        // Start timer to end the move loop if the target takes too long to send
        // the XdndStatus and XdndFinished messages.
        StartEndMoveLoopTimer();
        return;
      }

      EndMoveLoop();
      return;
    }

    if (negotiated_operation() != DragDropTypes::DRAG_NONE) {
      // Start timer to end the move loop if the target takes too long to send
      // an XdndFinished message. It is important that StartEndMoveLoopTimer()
      // is called before SendXdndDrop() because SendXdndDrop()
      // sends XdndFinished synchronously if the drop target is a Chrome
      // window.
      StartEndMoveLoopTimer();

      // We have negotiated an action with the other end.
      source_state_ = SourceState::kDropped;
      SendXdndDrop(source_current_window_);
      return;
    }
  }

  EndMoveLoop();
}

void XDragDropClient::HandleMoveLoopEnded() {
  if (source_current_window_ != x11::None) {
    SendXdndLeave(source_current_window_);
    source_current_window_ = x11::None;
  }
  ResetDragContext();
  StopRepeatMouseMoveTimer();
  StopEndMoveLoopTimer();
}

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

XID XDragDropClient::FindWindowFor(const gfx::Point& screen_point) {
  auto finder = delegate_->CreateWindowFinder();
  XID target = finder->FindWindowAt(screen_point);

  if (target == x11::None)
    return x11::None;

  // TODO(crbug/651775): The proxy window should be reported separately from the
  //     target window. XDND messages should be sent to the proxy, and their
  //     window field should point to the target.

  // Figure out which window we should test as XdndAware. If |target| has
  // XdndProxy, it will set that proxy on target, and if not, |target|'s
  // original value will remain.
  GetXIDProperty(target, kXdndProxy, &target);

  int version;
  if (GetIntProperty(target, kXdndAware, &version) &&
      version >= kMaxXdndVersion) {
    return target;
  }
  return x11::None;
}

void XDragDropClient::SendXClientEvent(XID xid, XEvent* xev) {
  DCHECK_EQ(ClientMessage, xev->type);

  // Don't send messages to the X11 message queue if we can help it.
  XDragDropClient* short_circuit = GetForWindow(xid);
  if (short_circuit) {
    Atom message_type = xev->xclient.message_type;
    if (message_type == gfx::GetAtom(kXdndEnter)) {
      short_circuit->OnXdndEnter(xev->xclient);
      return;
    } else if (message_type == gfx::GetAtom(kXdndLeave)) {
      short_circuit->OnXdndLeave(xev->xclient);
      return;
    } else if (message_type == gfx::GetAtom(kXdndPosition)) {
      short_circuit->OnXdndPosition(xev->xclient);
      return;
    } else if (message_type == gfx::GetAtom(kXdndStatus)) {
      short_circuit->OnXdndStatus(xev->xclient);
      return;
    } else if (message_type == gfx::GetAtom(kXdndFinished)) {
      short_circuit->OnXdndFinished(xev->xclient);
      return;
    } else if (message_type == gfx::GetAtom(kXdndDrop)) {
      short_circuit->OnXdndDrop(xev->xclient);
      return;
    }
  }

  // I don't understand why the GTK+ code is doing what it's doing here. It
  // goes out of its way to send the XEvent so that it receives a callback on
  // success or failure, and when it fails, it then sends an internal
  // GdkEvent about the failed drag. (And sending this message doesn't appear
  // to go through normal xlib machinery, but instead passes through the low
  // level xProto (the x11 wire format) that I don't understand.
  //
  // I'm unsure if I have to jump through those hoops, or if XSendEvent is
  // sufficient.
  XSendEvent(xdisplay_, xid, x11::False, 0, xev);
}

void XDragDropClient::SendXdndEnter(XID dest_window,
                                    const std::vector<Atom>& targets) {
  XEvent xev = PrepareXdndClientMessage(kXdndEnter, dest_window);
  xev.xclient.data.l[1] = (kMaxXdndVersion << 24);  // The version number.

  if (targets.size() > 3) {
    xev.xclient.data.l[1] |= 1;
    SetAtomArrayProperty(xwindow(), kXdndTypeList, "ATOM", targets);
  } else {
    // Pack the targets into the enter message.
    for (size_t i = 0; i < targets.size(); ++i)
      xev.xclient.data.l[2 + i] = targets[i];
  }

  SendXClientEvent(dest_window, &xev);
}

void XDragDropClient::SendXdndPosition(XID dest_window,
                                       const gfx::Point& screen_point,
                                       unsigned long event_time) {
  waiting_on_status_ = true;

  XEvent xev = PrepareXdndClientMessage(kXdndPosition, dest_window);
  xev.xclient.data.l[2] = (screen_point.x() << 16) | screen_point.y();
  xev.xclient.data.l[3] = event_time;
  xev.xclient.data.l[4] = XDragOperationToAtom(drag_operation_);
  SendXClientEvent(dest_window, &xev);

  // http://www.whatwg.org/specs/web-apps/current-work/multipage/dnd.html and
  // the Xdnd protocol both recommend that drag events should be sent
  // periodically.
  repeat_mouse_move_timer_.Start(
      FROM_HERE, base::TimeDelta::FromMilliseconds(350),
      base::BindOnce(&XDragDropClient::ProcessMouseMove, base::Unretained(this),
                     screen_point, event_time));
}

void XDragDropClient::SendXdndLeave(XID dest_window) {
  XEvent xev = PrepareXdndClientMessage(kXdndLeave, dest_window);
  SendXClientEvent(dest_window, &xev);
}

void XDragDropClient::SendXdndDrop(XID dest_window) {
  XEvent xev = PrepareXdndClientMessage(kXdndDrop, dest_window);
  xev.xclient.data.l[2] = x11::CurrentTime;
  SendXClientEvent(dest_window, &xev);
}

void XDragDropClient::EndMoveLoop() {
  delegate_->EndMoveLoop();
}

}  // namespace ui
