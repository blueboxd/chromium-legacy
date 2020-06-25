// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_OZONE_PLATFORM_WAYLAND_HOST_WAYLAND_WINDOW_DRAG_CONTROLLER_H_
#define UI_OZONE_PLATFORM_WAYLAND_HOST_WAYLAND_WINDOW_DRAG_CONTROLLER_H_

#include <cstdint>
#include <iosfwd>
#include <memory>
#include <string>

#include "base/callback_forward.h"
#include "base/memory/weak_ptr.h"
#include "ui/events/event.h"
#include "ui/events/platform/platform_event_dispatcher.h"
#include "ui/events/platform/scoped_event_dispatcher.h"
#include "ui/gfx/geometry/vector2d.h"
#include "ui/ozone/platform/wayland/gpu/wayland_surface_factory.h"
#include "ui/ozone/platform/wayland/host/wayland_data_device.h"
#include "ui/ozone/platform/wayland/host/wayland_data_source.h"
#include "ui/ozone/platform/wayland/host/wayland_pointer.h"
#include "ui/ozone/platform/wayland/host/wayland_toplevel_window.h"

namespace ui {

class WaylandConnection;
class WaylandDataDeviceManager;
class WaylandDataOffer;
class WaylandWindow;
class WaylandWindowManager;

// Drag controller implementation that drives window moving sessions (aka: tab
// dragging). Wayland Drag and Drop protocol is used, under the hood, to keep
// track of cursor location and surface focus.
//
// TODO(crbug.com/896640): Use drag icon to emulate window moving.
class WaylandWindowDragController : public WaylandDataDevice::DragDelegate,
                                    public WaylandDataSource::Delegate,
                                    public PlatformEventDispatcher {
 public:
  // Constants used to keep track of the drag controller state.
  enum class State {
    kIdle,      // No DnD session nor drag loop running.
    kAttached,  // DnD session ongoing but no drag loop running.
    kDetached,  // Drag loop running. ie: blocked in a Drag() call.
    kDropped    // Drop event was just received.
  };

  WaylandWindowDragController(WaylandConnection* connection,
                              WaylandDataDeviceManager* device_manager,
                              WaylandPointer::Delegate* pointer_delegate);
  WaylandWindowDragController(const WaylandWindowDragController&) = delete;
  WaylandWindowDragController& operator=(const WaylandWindowDragController&) =
      delete;
  ~WaylandWindowDragController() override;

  bool Drag(WaylandToplevelWindow* surface, const gfx::Vector2d& offset);
  void StopDragging();

  State state() const { return state_; }

 private:
  // WaylandDataDevice::DragDelegate:
  bool IsDragSource() const override;
  void DrawIcon() override;
  void OnDragOffer(std::unique_ptr<WaylandDataOffer> offer) override;
  void OnDragEnter(WaylandWindow* window,
                   const gfx::PointF& location,
                   uint32_t serial) override;
  void OnDragMotion(const gfx::PointF& location) override;
  void OnDragLeave() override;
  void OnDragDrop() override;

  // WaylandDataSource::Delegate
  void OnDataSourceFinish(bool completed) override;
  void OnDataSourceSend(const std::string& mime_type,
                        std::string* contents) override;

  // PlatformEventDispatcher
  bool CanDispatchEvent(const PlatformEvent& event) override;
  uint32_t DispatchEvent(const PlatformEvent& event) override;

  // Offers the focused window as available to be dragged. A new data source is
  // setup and the underlying DnD session is started, if not done yet.
  bool OfferWindow();
  // Handles drag/move mouse |event|, while in |kDetached| mode, forwarding it
  // as a bounds change event to the upper layer handlers.
  void HandleMotionEvent(MouseEvent* event);
  // Handles the mouse button release (i.e: drop). Dispatches the required
  // events and resets the internal state.
  void HandleDropAndResetState();
  // Registers as the top level PlatformEvent dispatcher and runs a nested
  // RunLoop, which blocks until the DnD session finishes.
  void RunLoop();
  // Unregisters the internal event dispatcher and asks to quit the nested loop.
  void QuitLoop();

  WaylandConnection* const connection_;
  WaylandDataDeviceManager* const data_device_manager_;
  WaylandDataDevice* const data_device_;
  WaylandWindowManager* const window_manager_;
  WaylandPointer::Delegate* const pointer_delegate_;

  State state_ = State::kIdle;
  WaylandToplevelWindow* dragged_window_ = nullptr;
  gfx::Vector2d drag_offset_;

  std::unique_ptr<WaylandDataSource> data_source_;
  std::unique_ptr<WaylandDataOffer> data_offer_;
  wl::Object<wl_surface> icon_surface_;

  std::unique_ptr<ScopedEventDispatcher> nested_dispatcher_;
  base::OnceClosure quit_loop_closure_;

  base::WeakPtrFactory<WaylandWindowDragController> weak_factory_{this};
};

// Stream operator so WaylandWindowDragController::State can be used in
// log/assertion statements.
std::ostream& operator<<(std::ostream& out,
                         WaylandWindowDragController::State state);

}  // namespace ui

#endif  // UI_OZONE_PLATFORM_WAYLAND_HOST_WAYLAND_WINDOW_DRAG_CONTROLLER_H_
