// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/ozone/platform/x11/x11_window_ozone.h"

#include "ui/base/dragdrop/os_exchange_data.h"
#include "ui/base/x/x11_os_exchange_data_provider.h"
#include "ui/base/x/x11_pointer_grab.h"
#include "ui/base/x/x11_topmost_window_finder.h"
#include "ui/events/event.h"
#include "ui/events/platform/scoped_event_dispatcher.h"
#include "ui/events/platform/x11/x11_event_source.h"
#include "ui/events/x/x11_window_event_manager.h"
#include "ui/ozone/platform/x11/x11_cursor_ozone.h"
#include "ui/platform_window/platform_window_delegate.h"
#include "ui/platform_window/platform_window_handler/wm_drop_handler.h"
#include "ui/platform_window/x11/x11_window_manager.h"

namespace ui {

namespace {

// TODO(crbug.com/1069469): Get rid of this class and use the better version
// when it is landed [1], [2]
//
// [1] https://chromium-review.googlesource.com/c/chromium/src/+/2159225
// [2] https://chromium-review.googlesource.com/c/chromium/src/+/2159228
class X11TopmostWindowFinder : public ui::EnumerateWindowsDelegate,
                               public ui::XTopmostWindowFinder {
 public:
  X11TopmostWindowFinder() = default;
  ~X11TopmostWindowFinder() override {}
  X11TopmostWindowFinder(const X11TopmostWindowFinder&) = delete;
  X11TopmostWindowFinder& operator=(const X11TopmostWindowFinder&) = delete;

  // Returns the topmost window at |screen_location_px|.
  XID FindWindowAt(const gfx::Point& screen_location_px) override {
    screen_location_px_ = screen_location_px;
    ui::EnumerateTopLevelWindows(this);
    return toplevel_;
  }

 private:
  // ui::EnumerateWindowsDelegate:
  bool ShouldStopIterating(XID xid) override {
    if (!ui::IsWindowVisible(xid))
      return false;

    auto* window = X11WindowManager::GetInstance()->GetWindow(
        static_cast<gfx::AcceleratedWidget>(xid));
    if (window) {
      if (ShouldStopIteratingAtLocalProcessWindow(window)) {
        toplevel_ = xid;
        return true;
      }
      return false;
    }

    if (ui::WindowContainsPoint(xid, screen_location_px_)) {
      toplevel_ = xid;
      return true;
    }
    return false;
  }

  bool ShouldStopIteratingAtLocalProcessWindow(X11Window* window) {
    DCHECK(window->IsVisible());

    if (!window->GetXRootWindowOuterBounds().Contains(screen_location_px_))
      return false;

    gfx::Rect window_bounds = window->GetOutterBounds();
    if (!window_bounds.Contains(screen_location_px_))
      return false;
    ::Region shape = window->shape();
    if (!shape)
      return false;
    gfx::Point window_point(screen_location_px_);
    window_point.Offset(-window_bounds.origin().x(),
                        -window_bounds.origin().y());
    return XPointInRegion(shape, window_point.x(), window_point.y()) ==
           x11::True;
  }

  gfx::Point screen_location_px_;
  XID toplevel_ = x11::None;
  XDisplay* display_ = gfx::GetXDisplay();
};

}  // namespace

X11WindowOzone::X11WindowOzone(PlatformWindowDelegate* delegate)
    : X11Window(delegate) {}

X11WindowOzone::~X11WindowOzone() = default;

void X11WindowOzone::SetCursor(PlatformCursor cursor) {
  X11CursorOzone* cursor_ozone = static_cast<X11CursorOzone*>(cursor);
  XWindow::SetCursor(cursor_ozone->xcursor());
}

void X11WindowOzone::Initialize(PlatformWindowInitProperties properties) {
  X11Window::Initialize(std::move(properties));
  // Set a class property key that allows |this| to be used for drag action.
  SetWmDragHandler(this, this);

  drag_drop_client_ =
      std::make_unique<XDragDropClient>(this, display(), window());
}

void X11WindowOzone::StartDrag(const ui::OSExchangeData& data,
                               int operation,
                               gfx::NativeCursor cursor,
                               base::OnceCallback<void(int)> callback) {
  DCHECK(drag_drop_client_);

  end_drag_callback_ = std::move(callback);
  drag_drop_client_->InitDrag(operation, &data);

  SetCapture();

  dragging_ = true;
}

std::unique_ptr<ui::XTopmostWindowFinder> X11WindowOzone::CreateWindowFinder() {
  return std::make_unique<X11TopmostWindowFinder>();
}

int X11WindowOzone::UpdateDrag(const gfx::Point& screen_point) {
  WmDropHandler* drop_handler = GetWmDropHandler(*this);
  if (!drop_handler)
    return ui::DragDropTypes::DRAG_NONE;
  // TODO: calculate the appropriate drag operation here.
  return drop_handler->OnDragMotion(gfx::PointF(screen_point),
                                    ui::DragDropTypes::DRAG_COPY);
}

void X11WindowOzone::UpdateCursor(
    ui::DragDropTypes::DragOperation negotiated_operation) {
  NOTIMPLEMENTED_LOG_ONCE();
}

void X11WindowOzone::OnBeginForeignDrag(XID window) {
  NOTIMPLEMENTED_LOG_ONCE();
}

void X11WindowOzone::OnEndForeignDrag() {
  NOTIMPLEMENTED_LOG_ONCE();
}

void X11WindowOzone::OnBeforeDragLeave() {
  NOTIMPLEMENTED_LOG_ONCE();
}

int X11WindowOzone::PerformDrop() {
  WmDropHandler* drop_handler = GetWmDropHandler(*this);
  if (!drop_handler)
    return ui::DragDropTypes::DRAG_NONE;

  DCHECK(drag_drop_client_);
  auto* target_current_context = drag_drop_client_->target_current_context();
  DCHECK(target_current_context);

  int drag_operation = ui::DragDropTypes::DRAG_NONE;

  drop_handler->OnDragDrop(std::make_unique<ui::OSExchangeData>(
      std::make_unique<ui::XOSExchangeDataProvider>(
          drag_drop_client_->xwindow(),
          target_current_context->fetched_targets())));
  return drag_operation;
}

void X11WindowOzone::EndMoveLoop() {
  std::move(end_drag_callback_).Run(0);
}

bool X11WindowOzone::DispatchDraggingUiEvent(ui::Event* event) {
  // Drag and drop have a priority over other processing.
  if (dragging_) {
    DCHECK(drag_drop_client_);

    switch (event->type()) {
      case ui::ET_MOUSE_MOVED:
      case ui::ET_MOUSE_DRAGGED: {
        drag_drop_client_->HandleMouseMovement(
            event->AsLocatedEvent()->root_location(),
            event->AsMouseEvent()->flags(),
            event->AsMouseEvent()->time_stamp());
        return true;
      }
      case ui::ET_MOUSE_RELEASED:
        if (!event->AsMouseEvent()->IsLeftMouseButton())
          break;
        // Assume that drags are being done with the left mouse button. Only
        // break the drag if the left mouse button was released.
        drag_drop_client_->HandleMouseReleased();
        dragging_ = false;
        ReleaseCapture();
        return true;
      case ui::ET_KEY_PRESSED:
        if (event->AsKeyEvent()->key_code() != ui::VKEY_ESCAPE)
          break;
        EndMoveLoop();
        drag_drop_client_->HandleMoveLoopEnded();
        dragging_ = false;
        ReleaseCapture();
        return true;
      default:
        break;
    }
  }
  return false;
}

void X11WindowOzone::OnXWindowSelectionEvent(XEvent* xev) {
  X11Window::OnXWindowSelectionEvent(xev);
  DCHECK(drag_drop_client_);
  drag_drop_client_->OnSelectionNotify(xev->xselection);
}

void X11WindowOzone::OnXWindowDragDropEvent(XEvent* xev) {
  X11Window::OnXWindowDragDropEvent(xev);
  DCHECK(drag_drop_client_);
  drag_drop_client_->HandleXdndEvent(xev->xclient);
}

}  // namespace ui
