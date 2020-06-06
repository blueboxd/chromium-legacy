// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/widget/desktop_aura/desktop_drag_drop_client_ozone.h"

#include <memory>
#include <utility>

#include "base/bind.h"
#include "base/run_loop.h"
#include "base/strings/string16.h"
#include "base/strings/utf_string_conversions.h"
#include "base/threading/thread_task_runner_handle.h"
#include "ui/aura/client/capture_client.h"
#include "ui/aura/client/cursor_client.h"
#include "ui/aura/client/drag_drop_delegate.h"
#include "ui/aura/window.h"
#include "ui/aura/window_tree_host.h"
#include "ui/base/clipboard/clipboard.h"
#include "ui/base/cursor/mojom/cursor_type.mojom-shared.h"
#include "ui/base/dragdrop/drag_drop_types.h"
#include "ui/base/dragdrop/drop_target_event.h"
#include "ui/platform_window/platform_window_delegate.h"
#include "ui/platform_window/platform_window_handler/wm_drag_handler.h"
#include "ui/views/widget/desktop_aura/desktop_native_cursor_manager.h"

namespace views {

namespace {

aura::Window* GetTargetWindow(aura::Window* root_window,
                              const gfx::Point& point) {
  gfx::Point root_location(point);
  root_window->GetHost()->ConvertScreenInPixelsToDIP(&root_location);
  return root_window->GetEventHandlerForPoint(root_location);
}

}  // namespace

DesktopDragDropClientOzone::DesktopDragDropClientOzone(
    aura::Window* root_window,
    views::DesktopNativeCursorManager* cursor_manager,
    ui::WmDragHandler* drag_handler)
    : root_window_(root_window),
      cursor_manager_(cursor_manager),
      drag_handler_(drag_handler) {}

DesktopDragDropClientOzone::~DesktopDragDropClientOzone() {
  ResetDragDropTarget();

  if (in_move_loop_)
    DragCancel();
}

int DesktopDragDropClientOzone::StartDragAndDrop(
    std::unique_ptr<ui::OSExchangeData> data,
    aura::Window* root_window,
    aura::Window* source_window,
    const gfx::Point& root_location,
    int operation,
    ui::DragDropTypes::DragEventSource source) {
  if (!drag_handler_)
    return ui::DragDropTypes::DragOperation::DRAG_NONE;

  DCHECK(!in_move_loop_);
  base::RunLoop run_loop(base::RunLoop::Type::kNestableTasksAllowed);
  quit_closure_ = run_loop.QuitClosure();

  // Chrome expects starting drag and drop to release capture.
  aura::Window* capture_window =
      aura::client::GetCaptureClient(root_window)->GetGlobalCaptureWindow();
  if (capture_window)
    capture_window->ReleaseCapture();

  aura::client::CursorClient* cursor_client =
      aura::client::GetCursorClient(root_window);

  auto initial_cursor = source_window->GetHost()->last_cursor();
  drag_operation_ = operation;
  if (cursor_client) {
    cursor_client->SetCursor(cursor_manager_->GetInitializedCursor(
        ui::mojom::CursorType::kGrabbing));
  }

  drag_handler_->StartDrag(
      *data.get(), operation, cursor_client->GetCursor(),
      base::BindOnce(&DesktopDragDropClientOzone::OnDragSessionClosed,
                     base::Unretained(this)));
  in_move_loop_ = true;
  run_loop.Run();

  if (cursor_client)
    cursor_client->SetCursor(initial_cursor);

  return drag_operation_;
}

void DesktopDragDropClientOzone::DragCancel() {
  QuitRunLoop();
}

bool DesktopDragDropClientOzone::IsDragDropInProgress() {
  return in_move_loop_;
}

void DesktopDragDropClientOzone::AddObserver(
    aura::client::DragDropClientObserver* observer) {
  NOTIMPLEMENTED_LOG_ONCE();
}

void DesktopDragDropClientOzone::RemoveObserver(
    aura::client::DragDropClientObserver* observer) {
  NOTIMPLEMENTED_LOG_ONCE();
}

void DesktopDragDropClientOzone::OnDragEnter(
    const gfx::PointF& point,
    std::unique_ptr<ui::OSExchangeData> data,
    int operation) {
  last_drag_point_ = point;
  drag_operation_ = operation;

  // If |data| is empty, we defer sending any events to the
  // |drag_drop_delegate_|.  All necessary events will be sent on dropping.
  if (!data)
    return;

  data_to_drop_ = std::move(data);
  UpdateTargetAndCreateDropEvent(point);
}

int DesktopDragDropClientOzone::OnDragMotion(const gfx::PointF& point,
                                             int operation) {
  last_drag_point_ = point;
  drag_operation_ = operation;
  int client_operation =
      ui::DragDropTypes::DRAG_COPY | ui::DragDropTypes::DRAG_MOVE;

  // If |data_to_drop_| has a valid data, ask |drag_drop_delegate_| about the
  // operation that it would accept.
  if (data_to_drop_) {
    std::unique_ptr<ui::DropTargetEvent> event =
        UpdateTargetAndCreateDropEvent(point);
    if (drag_drop_delegate_ && event)
      client_operation = drag_drop_delegate_->OnDragUpdated(*event);
  }
  return client_operation;
}

void DesktopDragDropClientOzone::OnDragDrop(
    std::unique_ptr<ui::OSExchangeData> data) {
  // If we didn't have |data_to_drop_|, then |drag_drop_delegate_| had never
  // been updated, and now it needs to receive deferred enter and update events
  // before handling the actual drop.
  const bool posponed_enter_and_update = !data_to_drop_;

  // If we had |data_to_drop_| already since the drag had entered the window,
  // then we don't expect new data to come now, and vice versa.
  DCHECK((data_to_drop_ && !data) || (!data_to_drop_ && data));
  if (!data_to_drop_)
    data_to_drop_ = std::move(data);

  // This will call the delegate's OnDragEntered if needed.
  auto event = UpdateTargetAndCreateDropEvent(last_drag_point_);
  if (drag_drop_delegate_ && event) {
    if (posponed_enter_and_update) {
      // TODO(https://crbug.com/1014860): deal with drop refusals.
      // The delegate's OnDragUpdated returns an operation that the delegate
      // would accept.  Normally the accepted operation would be propagated
      // properly, and if the delegate didn't accept it, the drop would never
      // be called, but in this scenario of postponed updates we send all events
      // at once.  Now we just drop, but perhaps we could call OnDragLeave
      // and quit?
      drag_drop_delegate_->OnDragUpdated(*event);
    }
    drag_operation_ =
        drag_drop_delegate_->OnPerformDrop(*event, std::move(data_to_drop_));
  }
  ResetDragDropTarget();
}

void DesktopDragDropClientOzone::OnDragLeave() {
  data_to_drop_.reset();
  ResetDragDropTarget();
}

void DesktopDragDropClientOzone::OnWindowDestroyed(aura::Window* window) {
  DCHECK_EQ(window, current_window_);

  current_window_->RemoveObserver(this);
  current_window_ = nullptr;
  drag_drop_delegate_ = nullptr;
}

void DesktopDragDropClientOzone::OnDragSessionClosed(int dnd_action) {
  drag_operation_ = dnd_action;
  QuitRunLoop();
}

void DesktopDragDropClientOzone::QuitRunLoop() {
  in_move_loop_ = false;
  if (quit_closure_.is_null())
    return;
  std::move(quit_closure_).Run();
}

std::unique_ptr<ui::DropTargetEvent>
DesktopDragDropClientOzone::UpdateTargetAndCreateDropEvent(
    const gfx::PointF& location) {
  const gfx::Point point(location.x(), location.y());
  aura::Window* window = GetTargetWindow(root_window_, point);
  if (!window) {
    ResetDragDropTarget();
    return nullptr;
  }

  auto* new_delegate = aura::client::GetDragDropDelegate(window);
  const bool delegate_has_changed = (new_delegate != drag_drop_delegate_);
  if (delegate_has_changed) {
    ResetDragDropTarget();
    drag_drop_delegate_ = new_delegate;
    current_window_ = window;
    current_window_->AddObserver(this);
  }

  if (!drag_drop_delegate_)
    return nullptr;

  gfx::Point root_location(location.x(), location.y());
  root_window_->GetHost()->ConvertScreenInPixelsToDIP(&root_location);
  gfx::PointF target_location(root_location);
  aura::Window::ConvertPointToTarget(root_window_, window, &target_location);

  auto event = std::make_unique<ui::DropTargetEvent>(
      *data_to_drop_, target_location, gfx::PointF(root_location),
      drag_operation_);
  if (delegate_has_changed)
    drag_drop_delegate_->OnDragEntered(*event);
  return event;
}

void DesktopDragDropClientOzone::ResetDragDropTarget() {
  if (drag_drop_delegate_) {
    drag_drop_delegate_->OnDragExited();
    drag_drop_delegate_ = nullptr;
  }
  if (current_window_) {
    current_window_->RemoveObserver(this);
    current_window_ = nullptr;
  }
}

}  // namespace views
