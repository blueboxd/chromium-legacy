// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromecast/graphics/cast_window_manager_aura.h"

#include "base/memory/ptr_util.h"
#include "build/build_config.h"
#include "chromecast/base/cast_features.h"
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
    bool use_external_frame_control)
    : WindowTreeHostPlatform(std::move(properties),
                             nullptr,
                             nullptr,
                             use_external_frame_control),
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
  CastLayoutManager(CastWindowManagerAura* window_manager,
                    aura::Window* parent);
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

  // Reorder child windows of the root window tree to reflect changes in layout.
  void ReorderChildWindows(aura::Window* changed_window);

  CastWindowManagerAura* const window_manager_;
  aura::Window* const parent_;

  DISALLOW_COPY_AND_ASSIGN(CastLayoutManager);
};

CastLayoutManager::CastLayoutManager(CastWindowManagerAura* window_manager,
                                     aura::Window* parent)
    : window_manager_(window_manager), parent_(parent) {
  DCHECK(window_manager_);
  DCHECK(parent_);
}

CastLayoutManager::~CastLayoutManager() {}

void CastLayoutManager::OnWindowResized() {
  // Invoked when the root window of the window tree host has been resized.
}

void CastLayoutManager::OnWindowAddedToLayout(aura::Window* child) {
  ReorderChildWindows(child);
}

void CastLayoutManager::OnWillRemoveWindowFromLayout(aura::Window* child) {}

void CastLayoutManager::OnWindowRemovedFromLayout(aura::Window* child) {
  ReorderChildWindows(child);
}

void CastLayoutManager::OnChildWindowVisibilityChanged(aura::Window* child,
                                                       bool visible) {
  ReorderChildWindows(child);
}

void CastLayoutManager::ReorderChildWindows(aura::Window* changed_window) {
  // Only update the window order if the child's parent matches the parent
  // window for this layout. If the child has no parent, then that means it
  // was removed from the layout and we should update the window ordering.
  if (changed_window->parent() && changed_window->parent() != parent_) {
    return;
  }

  // We set z-order here, because the window's ID could be set after the window
  // is added to the layout, particularly when using views::Widget to create the
  // window.  The window ID must be set prior to showing the window.  Since we
  // only process z-order on visibility changes for top-level windows, this
  // logic will execute infrequently.

  // Determine z-order relative to existing windows.
  aura::Window::Windows windows = parent_->children();
  std::stable_sort(windows.begin(), windows.end(),
                   [changed_window](aura::Window* lhs, aura::Window* rhs) {
                     // Promote |changed_window| to the top of the stack of
                     // windows with the same ID.
                     if (lhs->id() == rhs->id() && rhs == changed_window)
                       return true;

                     return lhs->id() < rhs->id();
                   });

  std::vector<CastWindowManager::WindowId> visible_window_order;
  for (size_t i = 0; i < windows.size(); ++i) {
    if (windows[i]->IsVisible()) {
      // static_cast is safe since the window ID value is originally derived
      // from CastWindowManager::WindowId.
      visible_window_order.push_back(
          static_cast<CastWindowManager::WindowId>(windows[i]->id()));
    }
    if (i == 0) {
      parent_->StackChildAtBottom(windows[i]);
    } else {
      parent_->StackChildAbove(windows[i], windows[i - 1]);
    }
  }

  window_manager_->OnWindowOrderChanged(visible_window_order);
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
  aura::Window* tree_window = window_tree_host_->window();
  tree_window->SetLayoutManager(new CastLayoutManager(this, tree_window));
  window_tree_host_->SetRootTransform(GetPrimaryDisplayRotationTransform());

  // Allow seeing through to the hardware video plane:
  window_tree_host_->compositor()->SetBackgroundColor(SK_ColorTRANSPARENT);

  focus_client_ = std::make_unique<CastFocusClientAura>();
  aura::client::SetFocusClient(tree_window, focus_client_.get());
  wm::SetActivationClient(tree_window, focus_client_.get());
  aura::client::SetWindowParentingClient(tree_window, this);
  capture_client_.reset(new aura::client::DefaultCaptureClient(tree_window));

  screen_position_client_ = std::make_unique<wm::DefaultScreenPositionClient>();

  // TODO(seantopping): Is |root_window| different from |tree_window|?
  aura::Window* root_window = tree_window->GetRootWindow();
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
  // No need for the edge swipe detector when side gestures are pass-through.
  if (!chromecast::IsFeatureEnabled(kEnableSideGesturePassThrough)) {
    side_swipe_detector_ = std::make_unique<SideSwipeDetector>(
        system_gesture_dispatcher_.get(), root_window);
  }
}

void CastWindowManagerAura::OnWindowOrderChanged(
    std::vector<WindowId> window_order) {
  window_order_.swap(window_order);
  for (auto& observer : observer_list_) {
    observer.WindowOrderChanged();
  }
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

void CastWindowManagerAura::AddObserver(Observer* observer) {
  observer_list_.AddObserver(observer);
}

void CastWindowManagerAura::RemoveObserver(Observer* observer) {
  observer_list_.RemoveObserver(observer);
}

gfx::NativeView CastWindowManagerAura::GetRootWindow() {
  Setup();
  return window_tree_host_->window();
}

std::vector<CastWindowManager::WindowId>
CastWindowManagerAura::GetWindowOrder() {
  return window_order_;
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
