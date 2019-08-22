// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromecast/graphics/cast_window_manager_aura.h"

#include "base/memory/ptr_util.h"
#include "build/build_config.h"
#include "chromecast/graphics/cast_focus_client_aura.h"
#include "chromecast/graphics/cast_touch_activity_observer.h"
#include "chromecast/graphics/cast_touch_event_gate.h"
#include "chromecast/graphics/gestures/cast_system_gesture_event_handler.h"
#include "chromecast/graphics/gestures/side_swipe_detector.h"
#include "ui/aura/client/default_capture_client.h"
#include "ui/aura/client/focus_change_observer.h"
#include "ui/aura/client/screen_position_client.h"
#include "ui/aura/env.h"
#include "ui/aura/layout_manager.h"
#include "ui/aura/null_window_targeter.h"
#include "ui/aura/window.h"
#include "ui/aura/window_tree_host_platform.h"
#include "ui/base/ime/init/input_method_factory.h"
#include "ui/base/ime/input_method.h"
#include "ui/display/display.h"
#include "ui/display/screen.h"
#include "ui/ozone/public/ozone_platform.h"
#include "ui/platform_window/platform_window_init_properties.h"
#include "ui/wm/core/default_screen_position_client.h"

#if defined(OS_FUCHSIA)
#include "ui/platform_window/fuchsia/initialize_presenter_api_view.h"
#endif

namespace chromecast {
namespace {

gfx::Transform GetPrimaryDisplayRotationTransform() {
  // NB: Using gfx::Transform::Rotate() introduces very small errors here
  // which are later exacerbated by use of gfx::EnclosingRect() in
  // WindowTreeHost::GetTransformedRootWindowBoundsInPixels().
  const gfx::Transform rotate_90(0.f, -1.f, 0.f, 0.f,  //
                                 1.f, 0.f, 0.f, 0.f,   //
                                 0.f, 0.f, 1.f, 0.f,   //
                                 0.f, 0.f, 0.f, 1.f);
  const gfx::Transform rotate_180 = rotate_90 * rotate_90;
  const gfx::Transform rotate_270 = rotate_180 * rotate_90;

  gfx::Transform translation;
  display::Display display(display::Screen::GetScreen()->GetPrimaryDisplay());
  switch (display.rotation()) {
    case display::Display::ROTATE_0:
      return translation;
    case display::Display::ROTATE_90:
      translation.Translate(display.bounds().height(), 0);
      return translation * rotate_90;
    case display::Display::ROTATE_180:
      translation.Translate(display.bounds().width(),
                            display.bounds().height());
      return translation * rotate_180;
    case display::Display::ROTATE_270:
      translation.Translate(0, display.bounds().width());
      return translation * rotate_270;
  }
}

gfx::Rect GetPrimaryDisplayHostBounds() {
  display::Display display(display::Screen::GetScreen()->GetPrimaryDisplay());
  gfx::Point display_origin_in_pixel = display.bounds().origin();
  gfx::Size display_size_in_pixel = display.GetSizeInPixel();
  switch (display.rotation()) {
    case display::Display::ROTATE_90:
    case display::Display::ROTATE_270:
      return gfx::Rect(display_origin_in_pixel,
                       gfx::Size(display_size_in_pixel.height(),
                                 display_size_in_pixel.width()));
    case display::Display::ROTATE_0:
    case display::Display::ROTATE_180:
      // default:
      return gfx::Rect(display_origin_in_pixel, display_size_in_pixel);
  }
}

}  // namespace

CastWindowTreeHost::CastWindowTreeHost(
    bool enable_input,
    ui::PlatformWindowInitProperties properties,
    ui::ExternalBeginFrameClient* external_begin_frame_client)
    : WindowTreeHostPlatform(std::move(properties),
                             nullptr,
                             nullptr,
                             external_begin_frame_client),
      enable_input_(enable_input) {
  if (!enable_input)
    window()->SetEventTargeter(std::make_unique<aura::NullWindowTargeter>());
}

CastWindowTreeHost::~CastWindowTreeHost() {}

void CastWindowTreeHost::DispatchEvent(ui::Event* event) {
  if (!enable_input_) {
    return;
  }

  WindowTreeHostPlatform::DispatchEvent(event);
}

gfx::Rect CastWindowTreeHost::GetTransformedRootWindowBoundsInPixels(
    const gfx::Size& host_size_in_pixels) const {
  gfx::RectF new_bounds(WindowTreeHost::GetTransformedRootWindowBoundsInPixels(
      host_size_in_pixels));
  new_bounds.set_origin(gfx::PointF());
  return gfx::ToEnclosingRect(new_bounds);
}

// A layout manager owned by the root window.
class CastLayoutManager : public aura::LayoutManager {
 public:
  CastLayoutManager();
  ~CastLayoutManager() override;

 private:
  // aura::LayoutManager implementation:
  void OnWindowResized() override;
  void OnWindowAddedToLayout(aura::Window* child) override;
  void OnWillRemoveWindowFromLayout(aura::Window* child) override;
  void OnWindowRemovedFromLayout(aura::Window* child) override;
  void OnChildWindowVisibilityChanged(aura::Window* child,
                                      bool visible) override;
  void SetChildBounds(aura::Window* child,
                      const gfx::Rect& requested_bounds) override;

  DISALLOW_COPY_AND_ASSIGN(CastLayoutManager);
};

CastLayoutManager::CastLayoutManager() {}
CastLayoutManager::~CastLayoutManager() {}

void CastLayoutManager::OnWindowResized() {
  // Invoked when the root window of the window tree host has been resized.
}

void CastLayoutManager::OnWindowAddedToLayout(aura::Window* child) {
  // Invoked when a window is added to the window tree host.
}

void CastLayoutManager::OnWillRemoveWindowFromLayout(aura::Window* child) {
  // Invoked when the root window of the window tree host will be removed.
}

void CastLayoutManager::OnWindowRemovedFromLayout(aura::Window* child) {
  // Invoked when the root window of the window tree host has been removed.
}

void CastLayoutManager::OnChildWindowVisibilityChanged(aura::Window* child,
                                                       bool visible) {
  aura::Window* parent = child->parent();
  if (!visible || !parent->IsRootWindow()) {
    return;
  }

  // We set z-order here, because the window's ID could be set after the window
  // is added to the layout, particularly when using views::Widget to create the
  // window.  The window ID must be set prior to showing the window.  Since we
  // only process z-order on visibility changes for top-level windows, this
  // logic will execute infrequently.

  // Determine z-order relative to existing windows.
  aura::Window::Windows windows = parent->children();
  std::stable_sort(windows.begin(), windows.end(),
                   [child](aura::Window* lhs, aura::Window* rhs) {
                     // Promote |child| to the top of the stack of windows with
                     // the same ID.
                     if (lhs->id() == rhs->id() && rhs == child)
                       return true;

                     return lhs->id() < rhs->id();
                   });

  for (size_t i = 0; i < windows.size(); ++i) {
    if (i == 0)
      parent->StackChildAtBottom(windows[i]);
    else
      parent->StackChildAbove(windows[i], windows[i - 1]);
  }
}

void CastLayoutManager::SetChildBounds(aura::Window* child,
                                       const gfx::Rect& requested_bounds) {
  SetChildBoundsDirect(child, requested_bounds);
}

CastWindowManagerAura::CastWindowManagerAura(bool enable_input)
    : enable_input_(enable_input) {}

CastWindowManagerAura::~CastWindowManagerAura() {
  TearDown();
}

void CastWindowManagerAura::Setup() {
  if (window_tree_host_) {
    return;
  }
  DCHECK(display::Screen::GetScreen());

  ui::InitializeInputMethodForTesting();

  gfx::Rect host_bounds = GetPrimaryDisplayHostBounds();
  ui::PlatformWindowInitProperties properties(host_bounds);

#if defined(OS_FUCHSIA)
  // When using Scenic Ozone platform we need to supply a view_token to the
  // window. This is not necessary when using the headless ozone platform.
  if (ui::OzonePlatform::GetInstance()
          ->GetPlatformProperties()
          .needs_view_token) {
    ui::fuchsia::InitializeViewTokenAndPresentView(&properties);
  }
#endif

  LOG(INFO) << "Starting window manager, bounds: " << host_bounds.ToString();
  CHECK(aura::Env::GetInstance());
  window_tree_host_ = std::make_unique<CastWindowTreeHost>(
      enable_input_, std::move(properties));
  window_tree_host_->InitHost();
  window_tree_host_->window()->SetLayoutManager(new CastLayoutManager());
  window_tree_host_->SetRootTransform(GetPrimaryDisplayRotationTransform());

  // Allow seeing through to the hardware video plane:
  window_tree_host_->compositor()->SetBackgroundColor(SK_ColorTRANSPARENT);

  focus_client_ = std::make_unique<CastFocusClientAura>();
  aura::client::SetFocusClient(window_tree_host_->window(),
                               focus_client_.get());
  wm::SetActivationClient(window_tree_host_->window(), focus_client_.get());
  aura::client::SetWindowParentingClient(window_tree_host_->window(), this);
  capture_client_.reset(
      new aura::client::DefaultCaptureClient(window_tree_host_->window()));

  screen_position_client_ = std::make_unique<wm::DefaultScreenPositionClient>();

  aura::Window* root_window = window_tree_host_->window()->GetRootWindow();
  aura::client::SetScreenPositionClient(root_window,
                                        screen_position_client_.get());

  window_tree_host_->Show();

  // Install the CastTouchEventGate before other event rewriters. It has to be
  // the first in the chain.
  event_gate_ = std::make_unique<CastTouchEventGate>(root_window);

  system_gesture_dispatcher_ = std::make_unique<CastSystemGestureDispatcher>();
  system_gesture_event_handler_ =
      std::make_unique<CastSystemGestureEventHandler>(
          system_gesture_dispatcher_.get(), root_window);
  side_swipe_detector_ = std::make_unique<SideSwipeDetector>(
      system_gesture_dispatcher_.get(), root_window);
}

CastWindowTreeHost* CastWindowManagerAura::window_tree_host() const {
  DCHECK(window_tree_host_);
  return window_tree_host_.get();
}

void CastWindowManagerAura::TearDown() {
  if (!window_tree_host_) {
    return;
  }
  event_gate_.reset();
  side_swipe_detector_.reset();
  capture_client_.reset();
  aura::client::SetWindowParentingClient(window_tree_host_->window(), nullptr);
  wm::SetActivationClient(window_tree_host_->window(), nullptr);
  aura::client::SetFocusClient(window_tree_host_->window(), nullptr);
  focus_client_.reset();
  system_gesture_event_handler_.reset();
  window_tree_host_.reset();
}

void CastWindowManagerAura::SetWindowId(gfx::NativeView window,
                                        WindowId window_id) {
  window->set_id(window_id);
}

void CastWindowManagerAura::InjectEvent(ui::Event* event) {
  if (!window_tree_host_) {
    return;
  }
  window_tree_host_->DispatchEvent(event);
}

gfx::NativeView CastWindowManagerAura::GetRootWindow() {
  Setup();
  return window_tree_host_->window();
}

aura::Window* CastWindowManagerAura::GetDefaultParent(aura::Window* window,
                                                      const gfx::Rect& bounds) {
  DCHECK(window_tree_host_);
  return window_tree_host_->window();
}

void CastWindowManagerAura::AddWindow(gfx::NativeView child) {
  LOG(INFO) << "Adding window: " << child->id() << ": " << child->GetName();
  Setup();

  DCHECK(child);
  aura::Window* parent = window_tree_host_->window();
  if (!parent->Contains(child)) {
    parent->AddChild(child);
  }
}

void CastWindowManagerAura::AddGestureHandler(CastGestureHandler* handler) {
  DCHECK(system_gesture_event_handler_);
  system_gesture_dispatcher_->AddGestureHandler(handler);
}

void CastWindowManagerAura::CastWindowManagerAura::RemoveGestureHandler(
    CastGestureHandler* handler) {
  DCHECK(system_gesture_event_handler_);
  system_gesture_dispatcher_->RemoveGestureHandler(handler);
}

CastGestureHandler* CastWindowManagerAura::GetGestureHandler() const {
  return system_gesture_dispatcher_.get();
}

void CastWindowManagerAura::SetTouchInputDisabled(bool disabled) {
  event_gate_->SetEnabled(disabled);
}

void CastWindowManagerAura::AddTouchActivityObserver(
    CastTouchActivityObserver* observer) {
  event_gate_->AddObserver(observer);
}

void CastWindowManagerAura::RemoveTouchActivityObserver(
    CastTouchActivityObserver* observer) {
  event_gate_->RemoveObserver(observer);
}

}  // namespace chromecast
