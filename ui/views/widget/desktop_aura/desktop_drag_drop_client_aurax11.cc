// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/widget/desktop_aura/desktop_drag_drop_client_aurax11.h"

#include <stddef.h>
#include <stdint.h>

#include <utility>

#include "base/macros.h"
#include "base/memory/ptr_util.h"
#include "base/metrics/histogram_macros.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "ui/aura/client/capture_client.h"
#include "ui/aura/client/drag_drop_client.h"
#include "ui/aura/client/drag_drop_delegate.h"
#include "ui/aura/window.h"
#include "ui/aura/window_tree_host.h"
#include "ui/base/dragdrop/drop_target_event.h"
#include "ui/base/dragdrop/os_exchange_data.h"
#include "ui/base/dragdrop/os_exchange_data_provider_aurax11.h"
#include "ui/base/layout.h"
#include "ui/base/mojom/cursor_type.mojom-shared.h"
#include "ui/base/x/selection_utils.h"
#include "ui/base/x/x11_drag_context.h"
#include "ui/base/x/x11_util.h"
#include "ui/display/screen.h"
#include "ui/events/event.h"
#include "ui/events/event_utils.h"
#include "ui/gfx/image/image_skia.h"
#include "ui/gfx/x/x11.h"
#include "ui/views/controls/image_view.h"
#include "ui/views/widget/desktop_aura/desktop_native_cursor_manager.h"
#include "ui/views/widget/desktop_aura/x11_topmost_window_finder.h"
#include "ui/views/widget/desktop_aura/x11_whole_screen_move_loop.h"
#include "ui/views/widget/widget.h"

using aura::client::DragDropDelegate;
using ui::OSExchangeData;

namespace {

// The minimum alpha before we declare a pixel transparent when searching in
// our source image.
constexpr uint32_t kMinAlpha = 32;

// |drag_widget_|'s opacity.
constexpr float kDragWidgetOpacity = .75f;

// Returns true if |image| has any visible regions (defined as having a pixel
// with alpha > 32).
bool IsValidDragImage(const gfx::ImageSkia& image) {
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

std::unique_ptr<views::Widget> CreateDragWidget(
    const gfx::ImageSkia& image,
    const gfx::Vector2d& drag_widget_offset) {
  auto widget = std::make_unique<views::Widget>();
  views::Widget::InitParams params(views::Widget::InitParams::TYPE_DRAG);
  if (ui::IsCompositingManagerPresent())
    params.opacity = views::Widget::InitParams::WindowOpacity::kTranslucent;
  else
    params.opacity = views::Widget::InitParams::WindowOpacity::kOpaque;
  params.ownership = views::Widget::InitParams::WIDGET_OWNS_NATIVE_WIDGET;
  params.accept_events = false;

  gfx::Point location =
      display::Screen::GetScreen()->GetCursorScreenPoint() - drag_widget_offset;
  params.bounds = gfx::Rect(location, image.size());
  widget->set_focus_on_creation(false);
  widget->set_frame_type(views::Widget::FrameType::kForceNative);
  widget->Init(std::move(params));
  if (params.opacity == views::Widget::InitParams::WindowOpacity::kTranslucent)
    widget->SetOpacity(kDragWidgetOpacity);
  widget->GetNativeWindow()->SetName("DragWindow");

  views::ImageView* image_view = new views::ImageView();
  image_view->SetImage(image);
  image_view->SetBoundsRect(gfx::Rect(image.size()));
  widget->SetContentsView(image_view);
  widget->Show();
  widget->GetNativeWindow()->layer()->SetFillsBoundsOpaquely(false);

  return widget;
}

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
    : XDragDropClient(this, display, window),
      root_window_(root_window),
      cursor_manager_(cursor_manager) {}

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

int DesktopDragDropClientAuraX11::StartDragAndDrop(
    std::unique_ptr<ui::OSExchangeData> data,
    aura::Window* root_window,
    aura::Window* source_window,
    const gfx::Point& /*screen_location*/,
    int operation,
    ui::DragDropTypes::DragEventSource source) {
  UMA_HISTOGRAM_ENUMERATION("Event.DragDrop.Start", source,
                            ui::DragDropTypes::DRAG_EVENT_SOURCE_COUNT);

  DCHECK(!g_current_drag_drop_client);
  g_current_drag_drop_client = this;

  InitDrag(operation, data.get());

  DCHECK(source_provider());
  gfx::ImageSkia drag_image = source_provider()->GetDragImage();
  if (IsValidDragImage(drag_image)) {
    drag_image_size_ = drag_image.size();
    drag_widget_offset_ = source_provider()->GetDragImageOffset();
    drag_widget_ = CreateDragWidget(drag_image, drag_widget_offset_);
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
                                             ui::mojom::CursorType::kGrabbing));

  if (alive) {
    auto resulting_operation = negotiated_operation();
    if (resulting_operation == ui::DragDropTypes::DRAG_NONE) {
      UMA_HISTOGRAM_ENUMERATION("Event.DragDrop.Cancel", source,
                                ui::DragDropTypes::DRAG_EVENT_SOURCE_COUNT);
    } else {
      UMA_HISTOGRAM_ENUMERATION("Event.DragDrop.Drop", source,
                                ui::DragDropTypes::DRAG_EVENT_SOURCE_COUNT);
    }
    drag_widget_.reset();
    g_current_drag_drop_client = nullptr;
    CleanupDrag();
    return resulting_operation;
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

bool DesktopDragDropClientAuraX11::DispatchXEvent(XEvent* event) {
  if (!target_current_context() ||
      event->xany.window != target_current_context()->source_window()) {
    return false;
  }
  return target_current_context()->DispatchXEvent(event);
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
  HandleMouseMovement(screen_point, flags, event_time);
}

void DesktopDragDropClientAuraX11::OnMouseReleased() {
  XDragDropClient::HandleMouseReleased();
}

void DesktopDragDropClientAuraX11::OnMoveLoopEnded() {
  XDragDropClient::HandleMoveLoopEnded();
}

std::unique_ptr<X11MoveLoop> DesktopDragDropClientAuraX11::CreateMoveLoop(
    X11MoveLoopDelegate* delegate) {
  return base::WrapUnique(new X11WholeScreenMoveLoop(this));
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

  DCHECK(target_current_context());
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
    (*event)->set_flags(ui::XGetMaskAsEventFlags());
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

std::unique_ptr<ui::XTopmostWindowFinder>
DesktopDragDropClientAuraX11::CreateWindowFinder() {
  return std::make_unique<X11TopmostWindowFinder>();
}

int DesktopDragDropClientAuraX11::UpdateDrag(const gfx::Point& screen_point) {
  // The drop target event holds a reference to data, that's why we have to hold
  // the data until the event is handled.
  std::unique_ptr<ui::OSExchangeData> data;
  std::unique_ptr<ui::DropTargetEvent> drop_target_event;
  DragDropDelegate* delegate = nullptr;
  DragTranslate(screen_point, &data, &drop_target_event, &delegate);
  int drag_operation =
      delegate ? drag_operation = delegate->OnDragUpdated(*drop_target_event)
               : ui::DragDropTypes::DRAG_NONE;
  UMA_HISTOGRAM_BOOLEAN("Event.DragDrop.AcceptDragUpdate",
                        drag_operation != ui::DragDropTypes::DRAG_NONE);

  return drag_operation;
}

void DesktopDragDropClientAuraX11::UpdateCursor(
    ui::DragDropTypes::DragOperation negotiated_operation) {
  ui::mojom::CursorType cursor_type = ui::mojom::CursorType::kNull;
  switch (negotiated_operation) {
    case ui::DragDropTypes::DRAG_NONE:
      cursor_type = ui::mojom::CursorType::kDndNone;
      break;
    case ui::DragDropTypes::DRAG_MOVE:
      cursor_type = ui::mojom::CursorType::kDndMove;
      break;
    case ui::DragDropTypes::DRAG_COPY:
      cursor_type = ui::mojom::CursorType::kDndCopy;
      break;
    case ui::DragDropTypes::DRAG_LINK:
      cursor_type = ui::mojom::CursorType::kDndLink;
      break;
  }
  move_loop_->UpdateCursor(cursor_manager_->GetInitializedCursor(cursor_type));
}

void DesktopDragDropClientAuraX11::OnBeginForeignDrag(XID window) {
  DCHECK(target_current_context());
  DCHECK(!target_current_context()->source_client());

  ui::X11EventSource::GetInstance()->AddXEventDispatcher(this);
  source_window_events_ =
      std::make_unique<ui::XScopedEventSelector>(window, PropertyChangeMask);
}

void DesktopDragDropClientAuraX11::OnEndForeignDrag() {
  DCHECK(target_current_context());
  DCHECK(!target_current_context()->source_client());

  ui::X11EventSource::GetInstance()->RemoveXEventDispatcher(this);
}

void DesktopDragDropClientAuraX11::OnBeforeDragLeave() {
  NotifyDragLeave();
}

int DesktopDragDropClientAuraX11::PerformDrop() {
  DCHECK(target_current_context());

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
        drop_event.set_flags(ui::XGetMaskAsEventFlags());
      }

      if (!IsDragDropInProgress()) {
        UMA_HISTOGRAM_COUNTS_1M("Event.DragDrop.ExternalOriginDrop", 1);
      }

      drag_operation = delegate->OnPerformDrop(drop_event, std::move(data));
    }

    target_window_->RemoveObserver(this);
    target_window_ = nullptr;
  }
  return drag_operation;
}

void DesktopDragDropClientAuraX11::EndMoveLoop() {
  move_loop_->EndMoveLoop();
}

}  // namespace views
