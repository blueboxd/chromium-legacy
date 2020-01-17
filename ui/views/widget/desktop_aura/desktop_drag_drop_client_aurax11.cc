// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/widget/desktop_aura/desktop_drag_drop_client_aurax11.h"

#include <stddef.h>
#include <stdint.h>

#include "base/bind.h"
#include "base/macros.h"
#include "base/memory/ptr_util.h"
#include "base/metrics/histogram_macros.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "ui/aura/client/capture_client.h"
#include "ui/aura/client/drag_drop_client.h"
#include "ui/aura/client/drag_drop_delegate.h"
#include "ui/aura/window.h"
#include "ui/aura/window_tree_host.h"
#include "ui/base/clipboard/clipboard_constants.h"
#include "ui/base/dragdrop/drop_target_event.h"
#include "ui/base/dragdrop/os_exchange_data.h"
#include "ui/base/dragdrop/os_exchange_data_provider_aurax11.h"
#include "ui/base/layout.h"
#include "ui/base/x/selection_utils.h"
#include "ui/base/x/x11_drag_context.h"
#include "ui/base/x/x11_util.h"
#include "ui/display/screen.h"
#include "ui/events/event.h"
#include "ui/events/event_utils.h"
#include "ui/events/platform/platform_event_source.h"
#include "ui/events/platform_event.h"
#include "ui/gfx/image/image_skia.h"
#include "ui/gfx/x/x11.h"
#include "ui/gfx/x/x11_atom_cache.h"
#include "ui/views/controls/image_view.h"
#include "ui/views/widget/desktop_aura/desktop_native_cursor_manager.h"
#include "ui/views/widget/desktop_aura/x11_topmost_window_finder.h"
#include "ui/views/widget/desktop_aura/x11_whole_screen_move_loop.h"
#include "ui/views/widget/widget.h"

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

using aura::client::DragDropDelegate;
using ui::OSExchangeData;

namespace {

constexpr int kWillAcceptDrop = 1;
constexpr int kWantFurtherPosEvents = 2;

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

// Message sent from an XDND target to the source in respose to an XdndDrop.
// The message must be sent whether the target acceepts the drop or not.
const char kXdndFinished[] = "XdndFinished";

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

int XGetModifiers() {
  XDisplay* display = gfx::GetXDisplay();

  XID root, child;
  int root_x, root_y, win_x, win_y;
  unsigned int mask;
  XQueryPointer(display,
                DefaultRootWindow(display),
                &root,
                &child,
                &root_x,
                &root_y,
                &win_x,
                &win_y,
                &mask);
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

// The minimum alpha before we declare a pixel transparent when searching in
// our source image.
constexpr uint32_t kMinAlpha = 32;

// |drag_widget_|'s opacity.
constexpr float kDragWidgetOpacity = .75f;

}  // namespace

namespace views {

DesktopDragDropClientAuraX11*
    DesktopDragDropClientAuraX11::g_current_drag_drop_client = nullptr;

///////////////////////////////////////////////////////////////////////////////

DesktopDragDropClientAuraX11::DesktopDragDropClientAuraX11(
    aura::Window* root_window,
    views::DesktopNativeCursorManager* cursor_manager,
    ::Display* display,
    XID window)
    : XDragDropClient(display, window),
      root_window_(root_window),
      cursor_manager_(cursor_manager) {
}

DesktopDragDropClientAuraX11::~DesktopDragDropClientAuraX11() {
  // This is necessary when the parent native widget gets destroyed while a drag
  // operation is in progress.
  move_loop_->EndMoveLoop();
  NotifyDragLeave();
  ResetDragContext();
}

void DesktopDragDropClientAuraX11::Init() {
  move_loop_ = CreateMoveLoop(this);
}

void DesktopDragDropClientAuraX11::OnXdndEnter(
    const XClientMessageEvent& event) {
  DVLOG(1) << "OnXdndEnter, version " << ((event.data.l[1] & 0xff000000) >> 24);

  XDragDropClient::OnXdndEnter(event);
  // If the XDragDropClient::OnXdndEnter had failed, we don't have the drag
  // context and should quit.
  if (!target_current_context())
    return;

  if (!target_current_context()->source_client()) {
    // The window doesn't have a DesktopDragDropClientAuraX11, which means it's
    // created by some other process.  Listen for messages on it.
    ui::PlatformEventSource::GetInstance()->AddPlatformEventDispatcher(this);
    source_window_events_ = std::make_unique<ui::XScopedEventSelector>(
        event.data.l[0], PropertyChangeMask);
  }

  // In the Windows implementation, we immediately call DesktopDropTargetWin::
  // Translate().  The XDND specification demands that we wait until we receive
  // an XdndPosition message before we use XConvertSelection or send an
  // XdndStatus message.
}

void DesktopDragDropClientAuraX11::OnXdndLeave(
    const XClientMessageEvent& event) {
  DVLOG(1) << "OnXdndLeave";

  NotifyDragLeave();

  // XDragDropClient::OnXdndLeave will reset the drag context, so do that as the
  // last thing.
  XDragDropClient::OnXdndLeave(event);
}

void DesktopDragDropClientAuraX11::OnXdndStatus(
    const XClientMessageEvent& event) {
  DVLOG(1) << "OnXdndStatus";

  XID source_window = event.data.l[0];

  if (source_window != source_current_window())
    return;

  if (source_state_ != SourceState::kPendingDrop &&
      source_state_ != SourceState::kOther) {
    return;
  }

  waiting_on_status_ = false;
  status_received_since_enter_ = true;

  if (event.data.l[1] & 1) {
    ::Atom atom_operation = event.data.l[4];
    negotiated_operation_ = ui::AtomToDragOperation(atom_operation);
  } else {
    negotiated_operation_ = ui::DragDropTypes::DRAG_NONE;
  }

  if (source_state_ == SourceState::kPendingDrop) {
    // We were waiting on the status message so we could send the XdndDrop.
    if (negotiated_operation_ == ui::DragDropTypes::DRAG_NONE) {
      move_loop_->EndMoveLoop();
      return;
    }
    source_state_ = SourceState::kDropped;
    SendXdndDrop(source_window);
    return;
  }

  ui::CursorType cursor_type = ui::CursorType::kNull;
  switch (negotiated_operation_) {
    case ui::DragDropTypes::DRAG_NONE:
      cursor_type = ui::CursorType::kDndNone;
      break;
    case ui::DragDropTypes::DRAG_MOVE:
      cursor_type = ui::CursorType::kDndMove;
      break;
    case ui::DragDropTypes::DRAG_COPY:
      cursor_type = ui::CursorType::kDndCopy;
      break;
    case ui::DragDropTypes::DRAG_LINK:
      cursor_type = ui::CursorType::kDndLink;
      break;
  }
  move_loop_->UpdateCursor(cursor_manager_->GetInitializedCursor(cursor_type));

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

void DesktopDragDropClientAuraX11::OnXdndFinished(
    const XClientMessageEvent& event) {
  DVLOG(1) << "OnXdndFinished";
  XID source_window = event.data.l[0];
  if (source_current_window() != source_window)
    return;

  // Clear |negotiated_operation_| if the drag was rejected.
  if ((event.data.l[1] & 1) == 0)
    negotiated_operation_ = ui::DragDropTypes::DRAG_NONE;

  // Clear |source_current_window_| to avoid sending XdndLeave upon ending the
  // move loop.
  set_source_current_window(x11::None);
  move_loop_->EndMoveLoop();
}

void DesktopDragDropClientAuraX11::OnXdndDrop(
    const XClientMessageEvent& event) {
  DVLOG(1) << "OnXdndDrop";

  XID source_window = event.data.l[0];

  int drag_operation = ui::DragDropTypes::DRAG_NONE;
  if (target_window_) {
    aura::client::DragDropDelegate* delegate =
        aura::client::GetDragDropDelegate(target_window_);
    if (delegate) {
      auto data(std::make_unique<ui::OSExchangeData>(
          std::make_unique<ui::OSExchangeDataProviderAuraX11>(
              xwindow(), target_current_context()->fetched_targets())));

      ui::DropTargetEvent drop_event(
          *data.get(), gfx::PointF(target_window_location_),
          gfx::PointF(target_window_root_location_),
          target_current_context()->GetDragOperation());
      if (target_current_context()->source_client()) {
        drop_event.set_flags(target_current_context()
                                 ->source_client()
                                 ->current_modifier_state());
      } else {
        drop_event.set_flags(XGetModifiers());
      }

      if (!IsDragDropInProgress()) {
        UMA_HISTOGRAM_COUNTS_1M("Event.DragDrop.ExternalOriginDrop", 1);
      }

      drag_operation = delegate->OnPerformDrop(drop_event, std::move(data));
    }

    target_window_->RemoveObserver(this);
    target_window_ = nullptr;
  }

  XEvent xev = PrepareXdndClientMessage(kXdndFinished, source_window);
  xev.xclient.data.l[1] = (drag_operation != 0) ? 1 : 0;
  xev.xclient.data.l[2] = ui::DragOperationToAtom(drag_operation);
  SendXClientEvent(source_window, &xev);
}

void DesktopDragDropClientAuraX11::OnSelectionNotify(
    const XSelectionEvent& xselection) {
  DVLOG(1) << "OnSelectionNotify";
  if (target_current_context())
    target_current_context()->OnSelectionNotify(xselection);

  // ICCCM requires us to delete the property passed into SelectionNotify.
  if (xselection.property != x11::None)
    XDeleteProperty(xdisplay(), xwindow(), xselection.property);
}

int DesktopDragDropClientAuraX11::StartDragAndDrop(
    std::unique_ptr<ui::OSExchangeData> data,
    aura::Window* root_window,
    aura::Window* source_window,
    const gfx::Point& screen_location,
    int operation,
    ui::DragDropTypes::DragEventSource source) {
  UMA_HISTOGRAM_ENUMERATION("Event.DragDrop.Start", source,
                            ui::DragDropTypes::DRAG_EVENT_SOURCE_COUNT);

  set_source_current_window(x11::None);
  DCHECK(!g_current_drag_drop_client);
  g_current_drag_drop_client = this;
  waiting_on_status_ = false;
  next_position_message_.reset();
  status_received_since_enter_ = false;
  source_state_ = SourceState::kOther;
  drag_operation_ = operation;
  negotiated_operation_ = ui::DragDropTypes::DRAG_NONE;

  const ui::OSExchangeData::Provider* provider = &data->provider();
  source_provider_ = static_cast<const ui::OSExchangeDataProviderAuraX11*>(
      provider);

  source_provider_->TakeOwnershipOfSelection();

  std::vector<::Atom> actions = GetOfferedDragOperations();
  if (!source_provider_->file_contents_name().empty()) {
    actions.push_back(gfx::GetAtom(kXdndActionDirectSave));
    ui::SetStringProperty(
        xwindow(), gfx::GetAtom(kXdndDirectSave0),
        gfx::GetAtom(ui::kMimeTypeText),
        source_provider_->file_contents_name().AsUTF8Unsafe());
  }
  ui::SetAtomArrayProperty(xwindow(), kXdndActionList, "ATOM", actions);

  gfx::ImageSkia drag_image = source_provider_->GetDragImage();
  if (IsValidDragImage(drag_image)) {
    CreateDragWidget(drag_image);
    drag_widget_offset_ = source_provider_->GetDragImageOffset();
  }

  // Chrome expects starting drag and drop to release capture.
  aura::Window* capture_window =
      aura::client::GetCaptureClient(root_window)->GetGlobalCaptureWindow();
  if (capture_window)
    capture_window->ReleaseCapture();

  // It is possible for the DesktopWindowTreeHostX11 to be destroyed during the
  // move loop, which would also destroy this drag-client. So keep track of
  // whether it is alive after the drag ends.
  base::WeakPtr<DesktopDragDropClientAuraX11> alive(
      weak_ptr_factory_.GetWeakPtr());

  // Windows has a specific method, DoDragDrop(), which performs the entire
  // drag. We have to emulate this, so we spin off a nested runloop which will
  // track all cursor movement and reroute events to a specific handler.
  move_loop_->RunMoveLoop(source_window, cursor_manager_->GetInitializedCursor(
                                             ui::CursorType::kGrabbing));

  if (alive) {
    if (negotiated_operation_ == ui::DragDropTypes::DRAG_NONE) {
      UMA_HISTOGRAM_ENUMERATION("Event.DragDrop.Cancel", source,
                                ui::DragDropTypes::DRAG_EVENT_SOURCE_COUNT);
    } else {
      UMA_HISTOGRAM_ENUMERATION("Event.DragDrop.Drop", source,
                                ui::DragDropTypes::DRAG_EVENT_SOURCE_COUNT);
    }
    drag_widget_.reset();

    source_provider_ = nullptr;
    g_current_drag_drop_client = nullptr;
    drag_operation_ = 0;
    XDeleteProperty(xdisplay(), xwindow(), gfx::GetAtom(kXdndActionList));
    XDeleteProperty(xdisplay(), xwindow(), gfx::GetAtom(kXdndDirectSave0));

    return negotiated_operation_;
  }
  UMA_HISTOGRAM_ENUMERATION("Event.DragDrop.Cancel", source,
                            ui::DragDropTypes::DRAG_EVENT_SOURCE_COUNT);
  return ui::DragDropTypes::DRAG_NONE;
}

void DesktopDragDropClientAuraX11::DragCancel() {
  move_loop_->EndMoveLoop();
}

bool DesktopDragDropClientAuraX11::IsDragDropInProgress() {
  return !!g_current_drag_drop_client;
}

void DesktopDragDropClientAuraX11::AddObserver(
    aura::client::DragDropClientObserver* observer) {
  NOTIMPLEMENTED();
}

void DesktopDragDropClientAuraX11::RemoveObserver(
    aura::client::DragDropClientObserver* observer) {
  NOTIMPLEMENTED();
}

bool DesktopDragDropClientAuraX11::CanDispatchEvent(
    const ui::PlatformEvent& event) {
  return target_current_context() &&
         event->xany.window == target_current_context()->source_window();
}

uint32_t DesktopDragDropClientAuraX11::DispatchEvent(
    const ui::PlatformEvent& event) {
  DCHECK(target_current_context());

  if (target_current_context()->DispatchXEvent(event))
    return ui::POST_DISPATCH_STOP_PROPAGATION;
  return ui::POST_DISPATCH_NONE;
}

void DesktopDragDropClientAuraX11::OnWindowDestroyed(aura::Window* window) {
  DCHECK_EQ(target_window_, window);
  target_window_ = nullptr;
}

void DesktopDragDropClientAuraX11::OnMouseMovement(
    const gfx::Point& screen_point,
    int flags,
    base::TimeTicks event_time) {
  if (drag_widget_.get()) {
    float scale_factor =
        ui::GetScaleFactorForNativeView(drag_widget_->GetNativeWindow());
    gfx::Point scaled_point =
        gfx::ScaleToRoundedPoint(screen_point, 1.f / scale_factor);
    drag_widget_->SetBounds(
        gfx::Rect(scaled_point - drag_widget_offset_, drag_image_size_));
    drag_widget_->StackAtTop();
  }

  const int kModifiers = ui::EF_SHIFT_DOWN | ui::EF_CONTROL_DOWN |
                         ui::EF_ALT_DOWN | ui::EF_COMMAND_DOWN |
                         ui::EF_LEFT_MOUSE_BUTTON |
                         ui::EF_MIDDLE_MOUSE_BUTTON |
                         ui::EF_RIGHT_MOUSE_BUTTON;
  current_modifier_state_ = flags & kModifiers;

  repeat_mouse_move_timer_.Stop();
  ProcessMouseMove(screen_point,
                   (event_time - base::TimeTicks()).InMilliseconds());
}

void DesktopDragDropClientAuraX11::OnMouseReleased() {
  repeat_mouse_move_timer_.Stop();

  if (source_state_ != SourceState::kOther) {
    // The user has previously released the mouse and is clicking in
    // frustration.
    move_loop_->EndMoveLoop();
    return;
  }

  if (source_current_window() != x11::None) {
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

      move_loop_->EndMoveLoop();
      return;
    }

    if (negotiated_operation_ != ui::DragDropTypes::DRAG_NONE) {
      // Start timer to end the move loop if the target takes too long to send
      // an XdndFinished message. It is important that StartEndMoveLoopTimer()
      // is called before SendXdndDrop() because SendXdndDrop()
      // sends XdndFinished synchronously if the drop target is a Chrome
      // window.
      StartEndMoveLoopTimer();

      // We have negotiated an action with the other end.
      source_state_ = SourceState::kDropped;
      SendXdndDrop(source_current_window());
      return;
    }
  }

  move_loop_->EndMoveLoop();
}

void DesktopDragDropClientAuraX11::OnMoveLoopEnded() {
  if (source_current_window() != x11::None) {
    SendXdndLeave(source_current_window());
    set_source_current_window(x11::None);
  }
  ResetDragContext();
  repeat_mouse_move_timer_.Stop();
  end_move_loop_timer_.Stop();
}

std::unique_ptr<X11MoveLoop> DesktopDragDropClientAuraX11::CreateMoveLoop(
    X11MoveLoopDelegate* delegate) {
  return base::WrapUnique(new X11WholeScreenMoveLoop(this));
}

void DesktopDragDropClientAuraX11::ProcessMouseMove(
    const gfx::Point& screen_point,
    unsigned long event_time) {
  if (source_state_ != SourceState::kOther)
    return;

  // Find the current window the cursor is over.
  ::Window dest_window = FindWindowFor(screen_point);

  if (source_current_window() != dest_window) {
    if (source_current_window() != x11::None)
      SendXdndLeave(source_current_window());

    set_source_current_window(dest_window);
    waiting_on_status_ = false;
    next_position_message_.reset();
    status_received_since_enter_ = false;
    negotiated_operation_ = ui::DragDropTypes::DRAG_NONE;

    if (source_current_window() != x11::None) {
      std::vector<Atom> targets;
      source_provider_->RetrieveTargets(&targets);
      SendXdndEnter(source_current_window(), targets);
    }
  }

  if (source_current_window() != x11::None) {
    if (waiting_on_status_) {
      next_position_message_ =
          std::make_unique<std::pair<gfx::Point, unsigned long>>(screen_point,
                                                                 event_time);
    } else {
      SendXdndPosition(dest_window, screen_point, event_time);
    }
  }
}

void DesktopDragDropClientAuraX11::StartEndMoveLoopTimer() {
  end_move_loop_timer_.Start(FROM_HERE, base::TimeDelta::FromMilliseconds(1000),
                             this, &DesktopDragDropClientAuraX11::EndMoveLoop);
}

void DesktopDragDropClientAuraX11::EndMoveLoop() {
  move_loop_->EndMoveLoop();
}

void DesktopDragDropClientAuraX11::DragTranslate(
    const gfx::Point& root_window_location,
    std::unique_ptr<ui::OSExchangeData>* data,
    std::unique_ptr<ui::DropTargetEvent>* event,
    aura::client::DragDropDelegate** delegate) {
  gfx::Point root_location = root_window_location;
  root_window_->GetHost()->ConvertScreenInPixelsToDIP(&root_location);
  aura::Window* target_window =
      root_window_->GetEventHandlerForPoint(root_location);
  bool target_window_changed = false;
  if (target_window != target_window_) {
    if (target_window_)
      NotifyDragLeave();
    target_window_ = target_window;
    if (target_window_)
      target_window_->AddObserver(this);
    target_window_changed = true;
  }
  *delegate = nullptr;
  if (!target_window_)
    return;
  *delegate = aura::client::GetDragDropDelegate(target_window_);
  if (!*delegate)
    return;

  *data = std::make_unique<OSExchangeData>(
      std::make_unique<ui::OSExchangeDataProviderAuraX11>(
          xwindow(), target_current_context()->fetched_targets()));
  gfx::Point location = root_location;
  aura::Window::ConvertPointToTarget(root_window_, target_window_, &location);

  target_window_location_ = location;
  target_window_root_location_ = root_location;

  int drag_op = target_current_context()->GetDragOperation();
  // KDE-based file browsers such as Dolphin change the drag operation depending
  // on whether alt/ctrl/shift was pressed. However once Chromium gets control
  // over the X11 events, the source application does no longer receive X11
  // events for key modifier changes, so the dnd operation gets stuck in an
  // incorrect state. Blink can only dnd-open files of type DRAG_COPY, so the
  // DRAG_COPY mask is added if the dnd object is a file.
  if (drag_op & (ui::DragDropTypes::DRAG_MOVE | ui::DragDropTypes::DRAG_LINK) &&
      data->get()->HasFile()) {
    drag_op |= ui::DragDropTypes::DRAG_COPY;
  }

  *event = std::make_unique<ui::DropTargetEvent>(
      *(data->get()), gfx::PointF(location), gfx::PointF(root_location),
      drag_op);
  if (target_current_context()->source_client()) {
    (*event)->set_flags(
        target_current_context()->source_client()->current_modifier_state());
  } else {
    (*event)->set_flags(XGetModifiers());
  }
  if (target_window_changed)
    (*delegate)->OnDragEntered(*event->get());
}

void DesktopDragDropClientAuraX11::NotifyDragLeave() {
  if (!target_window_)
    return;
  DragDropDelegate* delegate =
      aura::client::GetDragDropDelegate(target_window_);
  if (delegate)
    delegate->OnDragExited();
  target_window_->RemoveObserver(this);
  target_window_ = nullptr;
}

ui::SelectionFormatMap DesktopDragDropClientAuraX11::GetFormatMap() const {
  return source_provider_ ? source_provider_->GetFormatMap()
                          : ui::SelectionFormatMap();
}

void DesktopDragDropClientAuraX11::CompleteXdndPosition(
    ::Window source_window,
    const gfx::Point& screen_point) {
  int drag_operation = ui::DragDropTypes::DRAG_NONE;
  std::unique_ptr<ui::OSExchangeData> data;
  std::unique_ptr<ui::DropTargetEvent> drop_target_event;
  DragDropDelegate* delegate = nullptr;
  DragTranslate(screen_point, &data, &drop_target_event, &delegate);
  if (delegate)
    drag_operation = delegate->OnDragUpdated(*drop_target_event);
  UMA_HISTOGRAM_BOOLEAN("Event.DragDrop.AcceptDragUpdate",
                        drag_operation != ui::DragDropTypes::DRAG_NONE);

  // Sends an XdndStatus message back to the source_window. l[2,3]
  // theoretically represent an area in the window where the current action is
  // the same as what we're returning, but I can't find any implementation that
  // actually making use of this. A client can return (0, 0) and/or set the
  // first bit of l[1] to disable the feature, and it appears that gtk neither
  // sets this nor respects it if set.
  XEvent xev = PrepareXdndClientMessage(kXdndStatus, source_window);
  xev.xclient.data.l[1] = (drag_operation != 0) ?
      (kWantFurtherPosEvents | kWillAcceptDrop) : 0;
  xev.xclient.data.l[4] = ui::DragOperationToAtom(drag_operation);
  SendXClientEvent(source_window, &xev);
}

void DesktopDragDropClientAuraX11::SendXdndPosition(
    ::Window dest_window,
    const gfx::Point& screen_point,
    unsigned long event_time) {
  waiting_on_status_ = true;

  XEvent xev = PrepareXdndClientMessage(kXdndPosition, dest_window);
  xev.xclient.data.l[2] = (screen_point.x() << 16) | screen_point.y();
  xev.xclient.data.l[3] = event_time;
  xev.xclient.data.l[4] = ui::DragOperationToAtom(drag_operation_);
  SendXClientEvent(dest_window, &xev);

  // http://www.whatwg.org/specs/web-apps/current-work/multipage/dnd.html and
  // the Xdnd protocol both recommend that drag events should be sent
  // periodically.
  repeat_mouse_move_timer_.Start(
      FROM_HERE, base::TimeDelta::FromMilliseconds(350),
      base::BindOnce(&DesktopDragDropClientAuraX11::ProcessMouseMove,
                     base::Unretained(this), screen_point, event_time));
}

void DesktopDragDropClientAuraX11::CreateDragWidget(
    const gfx::ImageSkia& image) {
  Widget* widget = new Widget;
  Widget::InitParams params(Widget::InitParams::TYPE_DRAG);
  if (ui::IsCompositingManagerPresent())
    params.opacity = Widget::InitParams::WindowOpacity::kTranslucent;
  else
    params.opacity = Widget::InitParams::WindowOpacity::kOpaque;
  params.ownership = Widget::InitParams::WIDGET_OWNS_NATIVE_WIDGET;
  params.accept_events = false;

  gfx::Point location = display::Screen::GetScreen()->GetCursorScreenPoint() -
                        drag_widget_offset_;
  params.bounds = gfx::Rect(location, image.size());
  widget->set_focus_on_creation(false);
  widget->set_frame_type(Widget::FrameType::kForceNative);
  widget->Init(std::move(params));
  if (params.opacity == Widget::InitParams::WindowOpacity::kTranslucent)
    widget->SetOpacity(kDragWidgetOpacity);
  widget->GetNativeWindow()->SetName("DragWindow");

  drag_image_size_ = image.size();
  ImageView* image_view = new ImageView();
  image_view->SetImage(image);
  image_view->SetBoundsRect(gfx::Rect(drag_image_size_));
  widget->SetContentsView(image_view);
  widget->Show();
  widget->GetNativeWindow()->layer()->SetFillsBoundsOpaquely(false);

  drag_widget_.reset(widget);
}

bool DesktopDragDropClientAuraX11::IsValidDragImage(
    const gfx::ImageSkia& image) {
  if (image.isNull())
    return false;

  // Because we need a GL context per window, we do a quick check so that we
  // don't make another context if the window would just be displaying a mostly
  // transparent image.
  const SkBitmap* in_bitmap = image.bitmap();
  for (int y = 0; y < in_bitmap->height(); ++y) {
    uint32_t* in_row = in_bitmap->getAddr32(0, y);

    for (int x = 0; x < in_bitmap->width(); ++x) {
      if (SkColorGetA(in_row[x]) > kMinAlpha)
        return true;
    }
  }

  return false;
}

void DesktopDragDropClientAuraX11::ResetDragContext() {
  if (!target_current_context())
    return;
  if (!target_current_context()->source_client()) {
    ui::PlatformEventSource::GetInstance()->RemovePlatformEventDispatcher(this);
  }

  // XDragDropClient::ResetDragContext will reset target_current_context().
  XDragDropClient::ResetDragContext();
}

std::unique_ptr<ui::XTopmostWindowFinder>
DesktopDragDropClientAuraX11::CreateWindowFinder() {
  return std::make_unique<X11TopmostWindowFinder>();
}

}  // namespace views
