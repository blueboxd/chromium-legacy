// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/ozone/platform/wayland/host/wayland_window_drag_controller.h"

#include <cstdint>
#include <memory>
#include <ostream>
#include <utility>

#include "base/callback.h"
#include "base/check.h"
#include "base/memory/weak_ptr.h"
#include "base/message_loop/message_loop_current.h"
#include "base/notreached.h"
#include "base/run_loop.h"
#include "ui/base/dragdrop/drag_drop_types.h"
#include "ui/events/event.h"
#include "ui/events/event_constants.h"
#include "ui/events/platform/platform_event_dispatcher.h"
#include "ui/events/platform/platform_event_source.h"
#include "ui/events/platform/scoped_event_dispatcher.h"
#include "ui/events/platform_event.h"
#include "ui/events/types/event_type.h"
#include "ui/gfx/geometry/point_f.h"
#include "ui/gfx/geometry/vector2d.h"
#include "ui/ozone/platform/wayland/host/wayland_connection.h"
#include "ui/ozone/platform/wayland/host/wayland_cursor_position.h"
#include "ui/ozone/platform/wayland/host/wayland_data_device_manager.h"
#include "ui/ozone/platform/wayland/host/wayland_data_offer.h"
#include "ui/ozone/platform/wayland/host/wayland_data_source.h"
#include "ui/ozone/platform/wayland/host/wayland_event_source.h"
#include "ui/ozone/platform/wayland/host/wayland_pointer.h"
#include "ui/ozone/platform/wayland/host/wayland_surface.h"
#include "ui/ozone/platform/wayland/host/wayland_window.h"
#include "ui/ozone/platform/wayland/host/wayland_window_manager.h"

namespace ui {

namespace {

// Custom mime type used for window dragging DND sessions.
constexpr char kMimeTypeChromiumWindow[] = "chromium/x-window";

// DND action used in window dragging DND sessions.
constexpr uint32_t kDndActionWindowDrag =
    WL_DATA_DEVICE_MANAGER_DND_ACTION_MOVE;

}  // namespace

WaylandWindowDragController::WaylandWindowDragController(
    WaylandConnection* connection,
    WaylandDataDeviceManager* device_manager,
    WaylandPointer::Delegate* pointer_delegate)
    : connection_(connection),
      data_device_manager_(device_manager),
      data_device_(device_manager->GetDevice()),
      window_manager_(connection_->wayland_window_manager()),
      pointer_delegate_(pointer_delegate) {
  DCHECK(data_device_);
  DCHECK(pointer_delegate_);
}

WaylandWindowDragController::~WaylandWindowDragController() = default;

bool WaylandWindowDragController::Drag(WaylandToplevelWindow* window,
                                       const gfx::Vector2d& offset) {
  DCHECK_LE(state_, State::kAttached);
  DCHECK(window);

  if (!OfferWindow())
    return false;

  DCHECK_EQ(state_, State::kAttached);
  dragged_window_ = window;
  drag_offset_ = offset;
  RunLoop();

  DCHECK(state_ == State::kAttached || state_ == State::kDropped);
  bool dropped = state_ == State::kDropped;
  if (dropped)
    HandleDropAndResetState();
  return dropped;
}

void WaylandWindowDragController::StopDragging() {
  if (state_ != State::kDetached)
    return;

  VLOG(1) << "End drag loop requested. state=" << state_;

  // This function is supposed to be called to indicate that the window was just
  // snapped into a tab strip. So switch to |kAttached| state and ask to quit
  // the nested loop.
  state_ = State::kAttached;
  QuitLoop();
}

bool WaylandWindowDragController::IsDragSource() const {
  DCHECK(data_source_);
  return true;
}

// Icon drawing and update for window/tab dragging is handled by buffer manager.
void WaylandWindowDragController::DrawIcon() {}

void WaylandWindowDragController::OnDragOffer(
    std::unique_ptr<WaylandDataOffer> offer) {
  DCHECK_GE(state_, State::kAttached);
  DCHECK(offer);
  DCHECK(!data_offer_);
  data_offer_ = std::move(offer);
}

void WaylandWindowDragController::OnDragEnter(WaylandWindow* window,
                                              const gfx::PointF& location,
                                              uint32_t serial) {
  DCHECK_GE(state_, State::kAttached);
  DCHECK(window);

  // Forward focus change event to the input delegate, so other components, such
  // as WaylandScreen, are able to properly retrieve focus related info during
  // window dragging sesstions.
  pointer_delegate_->OnPointerFocusChanged(window, location);

  VLOG(1) << "OnEnter. widget=" << window->GetWidget();

  // Ensure this is a valid "window drag" offer.
  DCHECK(data_offer_);
  DCHECK_EQ(data_offer_->mime_types().size(), 1u);
  DCHECK_EQ(data_offer_->mime_types().front(), kMimeTypeChromiumWindow);

  // Accept the offer and set the dnd action.
  data_offer_->SetAction(kDndActionWindowDrag, kDndActionWindowDrag);
  data_offer_->Accept(serial, kMimeTypeChromiumWindow);
}

void WaylandWindowDragController::OnDragMotion(const gfx::PointF& location) {
  DCHECK_GE(state_, State::kAttached);
  VLOG(2) << "OnMotion. location=" << location.ToString();

  // Forward cursor location update info to the input handling delegate.
  pointer_delegate_->OnPointerMotionEvent(location);
}

void WaylandWindowDragController::OnDragLeave() {
  DCHECK_GE(state_, State::kAttached);
  DCHECK_LE(state_, State::kDetached);

  // In order to guarantee ET_MOUSE_RELEASED event is delivered once the DND
  // session finishes, the focused window is not reset here. This is similar to
  // the "implicit grab" behavior implemented by Wayland compositors for
  // wl_pointer events. Additionally, this makes it possible for the drag
  // controller to overcome deviations in the order that wl_data_source and
  // wl_pointer events arrive when the drop happens. For example, unlike Weston
  // and Sway, Gnome Shell <= 2.26 sends them in the following order:
  //
  // wl_data_device.leave >  wl_pointer.enter > wl_data_source.cancel/finish
  //
  // which would require hacky workarounds in HandleDropAndResetState function
  // to properly detect and handle such cases.

  VLOG(1) << "OnLeave";

  data_offer_.reset();
}

void WaylandWindowDragController::OnDragDrop() {
  // Not used for window dragging sessions. Handling of drop events is fully
  // done at OnDataSourceFinish function, i.e: wl_data_source::{cancel,finish}.
}

void WaylandWindowDragController::OnDataSourceFinish(bool completed) {
  DCHECK_GE(state_, State::kAttached);
  DCHECK(data_source_);

  VLOG(1) << "Drop received. state=" << state_;

  // Release DND objects.
  data_offer_.reset();
  data_source_.reset();
  icon_surface_.reset();
  dragged_window_ = nullptr;

  // Transition to |kDropped| state and determine the next action to take. If
  // drop happened while the move loop was running (i.e: kDetached), ask to quit
  // the loop, otherwise notify session end and reset state right away.
  State state_when_dropped = std::exchange(state_, State::kDropped);
  if (state_when_dropped == State::kDetached)
    QuitLoop();
  else
    HandleDropAndResetState();

  data_device_->ResetDragDelegate();
}

void WaylandWindowDragController::OnDataSourceSend(const std::string& mime_type,
                                                   std::string* contents) {
  // There is no actual data exchange in DnD window dragging sessions. Window
  // snapping, for example, is supposed to be handled at higher level UI layers.
}

bool WaylandWindowDragController::CanDispatchEvent(const PlatformEvent& event) {
  return state_ == State::kDetached;
}

uint32_t WaylandWindowDragController::DispatchEvent(
    const PlatformEvent& event) {
  DCHECK_EQ(state_, State::kDetached);
  DCHECK(base::MessageLoopCurrentForUI::IsSet());

  VLOG(2) << "Dispatch. event=" << event->GetName();

  if (event->type() == ET_MOUSE_MOVED || event->type() == ET_MOUSE_DRAGGED) {
    HandleMotionEvent(event->AsMouseEvent());
    return POST_DISPATCH_STOP_PROPAGATION;
  }
  return POST_DISPATCH_PERFORM_DEFAULT;
}

bool WaylandWindowDragController::OfferWindow() {
  DCHECK_LE(state_, State::kAttached);

  auto* window = window_manager_->GetCurrentFocusedWindow();
  if (!window) {
    LOG(ERROR) << "Failed to get focused window.";
    return false;
  }

  if (!data_source_)
    data_source_ = data_device_manager_->CreateSource(this);

  if (state_ == State::kIdle) {
    DCHECK(!icon_surface_);
    icon_surface_.reset(
        wl_compositor_create_surface(connection_->compositor()));

    VLOG(1) << "Starting DND session.";
    state_ = State::kAttached;
    data_source_->Offer({kMimeTypeChromiumWindow});
    data_source_->SetAction(DragDropTypes::DRAG_MOVE);
    data_device_->StartDrag(*data_source_, *window, icon_surface_.get(), this);
  }
  return true;
}

void WaylandWindowDragController::HandleMotionEvent(MouseEvent* event) {
  DCHECK_EQ(state_, State::kDetached);
  DCHECK(dragged_window_);
  DCHECK(event);

  // Update current cursor position, so it can be retrieved later on through
  // |Screen::GetCursorScreenPoint| API.
  int32_t scale = dragged_window_->buffer_scale();
  gfx::PointF scaled_location =
      gfx::ScalePoint(event->location_f(), scale, scale);
  connection_->wayland_cursor_position()->OnCursorPositionChanged(
      gfx::ToFlooredPoint(scaled_location));

  // Notify listeners about window bounds change (i.e: re-positioning) event.
  // To do so, set the new bounds as per the motion event location and the drag
  // offset. Note that setting a new location (i.e: bounds.origin()) for a
  // surface has no visual effect in ozone/wayland backend. Actual window
  // re-positioning during dragging session is done through the drag icon.
  gfx::Point new_location = event->location() - drag_offset_;
  gfx::Size size = dragged_window_->GetBounds().size();
  dragged_window_->SetBounds({new_location, size});
}

// Dispatch mouse release event (to tell clients that the drop just happened)
// clear focus and reset internal state. Must be called when the session is
// about to finish.
void WaylandWindowDragController::HandleDropAndResetState() {
  DCHECK_EQ(state_, State::kDropped);
  DCHECK(window_manager_->GetCurrentFocusedWindow());
  VLOG(1) << "Notifying drop. window="
          << window_manager_->GetCurrentFocusedWindow();

  EventFlags pointer_button = EF_LEFT_MOUSE_BUTTON;
  DCHECK(connection_->event_source()->IsPointerButtonPressed(pointer_button));
  pointer_delegate_->OnPointerButtonEvent(ET_MOUSE_RELEASED, pointer_button);

  state_ = State::kIdle;
}

void WaylandWindowDragController::RunLoop() {
  DCHECK_EQ(state_, State::kAttached);
  DCHECK(dragged_window_);

  VLOG(1) << "Starting drag loop. widget=" << dragged_window_->GetWidget();

  // TODO(crbug.com/896640): Handle cursor
  auto old_dispatcher = std::move(nested_dispatcher_);
  nested_dispatcher_ =
      PlatformEventSource::GetInstance()->OverrideDispatcher(this);

  base::WeakPtr<WaylandWindowDragController> alive(weak_factory_.GetWeakPtr());

  state_ = State::kDetached;
  base::RunLoop loop(base::RunLoop::Type::kNestableTasksAllowed);
  quit_loop_closure_ = loop.QuitClosure();
  loop.Run();

  if (!alive)
    return;

  nested_dispatcher_ = std::move(old_dispatcher);

  VLOG(1) << "Quitting drag loop " << state_;
}

void WaylandWindowDragController::QuitLoop() {
  DCHECK(!quit_loop_closure_.is_null());

  nested_dispatcher_.reset();
  std::move(quit_loop_closure_).Run();
}

std::ostream& operator<<(std::ostream& out,
                         WaylandWindowDragController::State state) {
  return out << static_cast<int>(state);
}

}  // namespace ui
