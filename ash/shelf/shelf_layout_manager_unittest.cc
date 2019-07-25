// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/shelf/shelf_layout_manager.h"

#include <memory>
#include <utility>

#include "ash/accelerators/accelerator_controller_impl.h"
#include "ash/accelerators/accelerator_table.h"
#include "ash/accessibility/test_accessibility_controller_client.h"
#include "ash/app_list/app_list_controller_impl.h"
#include "ash/app_list/test/app_list_test_helper.h"
#include "ash/app_list/views/app_list_view.h"
#include "ash/focus_cycler.h"
#include "ash/home_screen/home_launcher_gesture_handler.h"
#include "ash/home_screen/home_screen_controller.h"
#include "ash/keyboard/ui/keyboard_ui.h"
#include "ash/keyboard/ui/keyboard_ui_controller.h"
#include "ash/keyboard/ui/keyboard_util.h"
#include "ash/public/cpp/ash_switches.h"
#include "ash/public/cpp/immersive/immersive_fullscreen_controller_test_api.h"
#include "ash/public/cpp/keyboard/keyboard_controller_observer.h"
#include "ash/public/cpp/shelf_item.h"
#include "ash/public/cpp/shell_window_ids.h"
#include "ash/public/cpp/test/shell_test_api.h"
#include "ash/public/cpp/wallpaper_controller_observer.h"
#include "ash/public/cpp/window_properties.h"
#include "ash/root_window_controller.h"
#include "ash/screen_util.h"
#include "ash/session/session_controller_impl.h"
#include "ash/shelf/home_button.h"
#include "ash/shelf/shelf.h"
#include "ash/shelf/shelf_constants.h"
#include "ash/shelf/shelf_controller.h"
#include "ash/shelf/shelf_layout_manager_observer.h"
#include "ash/shelf/shelf_view.h"
#include "ash/shelf/shelf_view_test_api.h"
#include "ash/shelf/shelf_widget.h"
#include "ash/shell.h"
#include "ash/system/status_area_widget.h"
#include "ash/system/status_area_widget_test_helper.h"
#include "ash/system/unified/unified_system_tray.h"
#include "ash/test/ash_test_base.h"
#include "ash/wallpaper/wallpaper_controller_impl.h"
#include "ash/window_factory.h"
#include "ash/wm/lock_state_controller.h"
#include "ash/wm/overview/overview_controller.h"
#include "ash/wm/tablet_mode/tablet_mode_controller_test_api.h"
#include "ash/wm/window_state.h"
#include "ash/wm/window_util.h"
#include "ash/wm/wm_event.h"
#include "ash/wm/work_area_insets.h"
#include "ash/wm/workspace_controller.h"
#include "base/bind.h"
#include "base/command_line.h"
#include "base/run_loop.h"
#include "base/stl_util.h"
#include "base/test/metrics/user_action_tester.h"
#include "base/test/scoped_feature_list.h"
#include "ui/aura/client/aura_constants.h"
#include "ui/aura/client/window_parenting_client.h"
#include "ui/aura/window.h"
#include "ui/aura/window_event_dispatcher.h"
#include "ui/base/ui_base_switches.h"
#include "ui/compositor/layer.h"
#include "ui/compositor/layer_animator.h"
#include "ui/compositor/scoped_animation_duration_scale_mode.h"
#include "ui/display/display.h"
#include "ui/display/display_layout.h"
#include "ui/display/display_observer.h"
#include "ui/display/manager/display_manager.h"
#include "ui/display/screen.h"
#include "ui/display/test/display_manager_test_api.h"
#include "ui/events/gesture_detection/gesture_configuration.h"
#include "ui/events/test/event_generator.h"
#include "ui/views/view.h"
#include "ui/views/widget/widget.h"
#include "ui/wm/core/window_util.h"

namespace ash {
namespace {

void PressHomeButton() {
  Shell::Get()->app_list_controller()->OnHomeButtonPressed(
      display::Screen::GetScreen()->GetPrimaryDisplay().id(),
      app_list::AppListShowSource::kShelfButton, base::TimeTicks());
}

void StepWidgetLayerAnimatorToEnd(views::Widget* widget) {
  widget->GetNativeView()->layer()->GetAnimator()->Step(
      base::TimeTicks::Now() + base::TimeDelta::FromSeconds(1));
}

ShelfWidget* GetShelfWidget() {
  return AshTestBase::GetPrimaryShelf()->shelf_widget();
}

ShelfLayoutManager* GetShelfLayoutManager() {
  return AshTestBase::GetPrimaryShelf()->shelf_layout_manager();
}

gfx::Rect GetScreenAvailableBounds() {
  const WorkAreaInsets* const work_area =
      WorkAreaInsets::ForWindow(GetShelfWidget()->GetNativeWindow());
  gfx::Rect available_bounds = screen_util::GetDisplayBoundsWithShelf(
      GetShelfWidget()->GetNativeWindow());
  available_bounds.Inset(work_area->GetAccessibilityInsets());
  return available_bounds;
}

// Class which waits till the shelf finishes animating to the target size and
// counts the number of animation steps.
class ShelfAnimationWaiter : views::WidgetObserver {
 public:
  explicit ShelfAnimationWaiter(const gfx::Rect& target_bounds)
      : target_bounds_(target_bounds),
        animation_steps_(0),
        done_waiting_(false) {
    GetShelfWidget()->AddObserver(this);
  }

  ~ShelfAnimationWaiter() override { GetShelfWidget()->RemoveObserver(this); }

  // Wait till the shelf finishes animating to its expected bounds.
  void WaitTillDoneAnimating() {
    if (IsDoneAnimating())
      done_waiting_ = true;
    else
      base::RunLoop().Run();
  }

  // Returns true if the animation has completed and it was valid.
  bool WasValidAnimation() const {
    return done_waiting_ && animation_steps_ > 0;
  }

 private:
  // Returns true if shelf has finished animating to the target position.
  bool IsDoneAnimating() const {
    gfx::Rect current_bounds = GetShelfWidget()->GetWindowBoundsInScreen();
    return current_bounds.origin() == target_bounds_.origin();
  }

  // views::WidgetObserver override.
  void OnWidgetBoundsChanged(views::Widget* widget,
                             const gfx::Rect& new_bounds) override {
    if (done_waiting_)
      return;

    ++animation_steps_;
    if (IsDoneAnimating()) {
      done_waiting_ = true;
      base::RunLoop::QuitCurrentWhenIdleDeprecated();
    }
  }

  gfx::Rect target_bounds_;
  int animation_steps_;
  bool done_waiting_;

  DISALLOW_COPY_AND_ASSIGN(ShelfAnimationWaiter);
};

class ShelfDragCallback {
 public:
  ShelfDragCallback(const gfx::Rect& auto_hidden_shelf_bounds,
                    const gfx::Rect& visible_shelf_bounds)
      : auto_hidden_shelf_bounds_(auto_hidden_shelf_bounds),
        visible_shelf_bounds_(visible_shelf_bounds),
        was_visible_on_drag_start_(false) {
    EXPECT_EQ(auto_hidden_shelf_bounds_.size(), visible_shelf_bounds_.size());
  }

  virtual ~ShelfDragCallback() = default;

  void ProcessScroll(ui::EventType type, const gfx::Vector2dF& delta) {
    ProcessScrollInternal(type, delta, true);
  }

  void ProcessScrollNoBoundsCheck(ui::EventType type,
                                  const gfx::Vector2dF& delta) {
    ProcessScrollInternal(type, delta, false);
  }

  void ProcessScrollInternal(ui::EventType type,
                             const gfx::Vector2dF& delta,
                             bool bounds_check) {
    if (GetShelfLayoutManager()->visibility_state() == SHELF_HIDDEN)
      return;

    if (type == ui::ET_GESTURE_SCROLL_BEGIN) {
      scroll_ = gfx::Vector2dF();
      was_visible_on_drag_start_ = GetShelfLayoutManager()->IsVisible();
      return;
    }

    // The state of the shelf at the end of the gesture is tested separately.
    if (type == ui::ET_GESTURE_SCROLL_END)
      return;

    if (type == ui::ET_GESTURE_SCROLL_UPDATE)
      scroll_.Add(delta);

    Shelf* shelf = AshTestBase::GetPrimaryShelf();
    gfx::Rect shelf_bounds = GetShelfWidget()->GetWindowBoundsInScreen();

    float scroll_delta =
        GetShelfLayoutManager()->PrimaryAxisValue(scroll_.y(), scroll_.x());
    bool increasing_drag =
        GetShelfLayoutManager()->SelectValueForShelfAlignment(
            scroll_delta<0, scroll_delta> 0, scroll_delta < 0);
    const int shelf_size = GetShelfLayoutManager()->PrimaryAxisValue(
        shelf_bounds.height(), shelf_bounds.width());
    if (was_visible_on_drag_start_) {
      if (increasing_drag) {
        // If dragging inwards from the visible state, then the shelf should
        // 'overshoot', but not by more than the scroll delta.
        const int bounds_delta =
            GetShelfLayoutManager()->SelectValueForShelfAlignment(
                visible_shelf_bounds_.y() - shelf_bounds.y(),
                shelf_bounds.x() - visible_shelf_bounds_.x(),
                visible_shelf_bounds_.x() - shelf_bounds.x());
        EXPECT_GE(bounds_delta, 0);
        EXPECT_LE(bounds_delta, std::abs(scroll_delta));
      } else {
        // If dragging outwards from the visible state, then the shelf should
        // move out.
        if (SHELF_ALIGNMENT_BOTTOM == shelf->alignment())
          EXPECT_LE(visible_shelf_bounds_.y(), shelf_bounds.y());
        else if (SHELF_ALIGNMENT_LEFT == shelf->alignment())
          EXPECT_LE(shelf_bounds.x(), visible_shelf_bounds_.x());
        else if (SHELF_ALIGNMENT_RIGHT == shelf->alignment())
          EXPECT_LE(visible_shelf_bounds_.x(), shelf_bounds.x());
      }
    } else {
      // The shelf is invisible at the start of the drag.
      if (increasing_drag && bounds_check) {
        constexpr float kEpsilon = 1.f;
        // Moving the shelf into the screen.
        if (std::abs(scroll_delta) < shelf_size) {
          // Tests that the shelf sticks with the touch point during the drag
          // until the shelf is completely visible.
          if (SHELF_ALIGNMENT_BOTTOM == shelf->alignment()) {
            EXPECT_NEAR(shelf_bounds.y(),
                        auto_hidden_shelf_bounds_.y() +
                            kHiddenShelfInScreenPortion -
                            std::abs(scroll_delta),
                        kEpsilon);
          } else if (SHELF_ALIGNMENT_LEFT == shelf->alignment()) {
            EXPECT_NEAR(shelf_bounds.x(),
                        auto_hidden_shelf_bounds_.x() -
                            kHiddenShelfInScreenPortion +
                            std::abs(scroll_delta),
                        kEpsilon);
          } else if (SHELF_ALIGNMENT_RIGHT == shelf->alignment()) {
            EXPECT_NEAR(shelf_bounds.x(),
                        auto_hidden_shelf_bounds_.x() +
                            kHiddenShelfInScreenPortion -
                            std::abs(scroll_delta),
                        kEpsilon);
          }
        } else {
          // Tests that after the shelf is completely visible, the shelf starts
          // resisting the drag.
          if (SHELF_ALIGNMENT_BOTTOM == shelf->alignment()) {
            EXPECT_GT(shelf_bounds.y(), auto_hidden_shelf_bounds_.y() +
                                            kHiddenShelfInScreenPortion -
                                            std::abs(scroll_delta));
          } else if (SHELF_ALIGNMENT_LEFT == shelf->alignment()) {
            EXPECT_LT(shelf_bounds.x(), auto_hidden_shelf_bounds_.x() -
                                            kHiddenShelfInScreenPortion +
                                            std::abs(scroll_delta));
          } else if (SHELF_ALIGNMENT_RIGHT == shelf->alignment()) {
            EXPECT_GT(shelf_bounds.x(), auto_hidden_shelf_bounds_.x() +
                                            kHiddenShelfInScreenPortion -
                                            std::abs(scroll_delta));
          }
        }
      }
    }
  }

 private:
  const gfx::Rect auto_hidden_shelf_bounds_;
  const gfx::Rect visible_shelf_bounds_;
  gfx::Vector2dF scroll_;
  bool was_visible_on_drag_start_;

  DISALLOW_COPY_AND_ASSIGN(ShelfDragCallback);
};

class TestDisplayObserver : public display::DisplayObserver {
 public:
  TestDisplayObserver() { display::Screen::GetScreen()->AddObserver(this); }
  ~TestDisplayObserver() override {
    display::Screen::GetScreen()->RemoveObserver(this);
  }

  int metrics_change_count() const { return metrics_change_count_; }

 private:
  // ShelfLayoutManagerObserver:
  void OnDisplayMetricsChanged(const display::Display& display,
                               uint32_t changed_metrics) override {
    metrics_change_count_++;
  }

  int metrics_change_count_ = 0;

  DISALLOW_COPY_AND_ASSIGN(TestDisplayObserver);
};

class WallpaperShownWaiter : public WallpaperControllerObserver {
 public:
  WallpaperShownWaiter() {
    Shell::Get()->wallpaper_controller()->AddObserver(this);
  }

  ~WallpaperShownWaiter() override {
    Shell::Get()->wallpaper_controller()->RemoveObserver(this);
  }

  // Note this could only be called once because RunLoop would not run after
  // Quit is called. Create a new instance if there's need to wait again.
  void Wait() { run_loop_.Run(); }

 private:
  // WallpaperControllerObserver:
  void OnFirstWallpaperShown() override { run_loop_.Quit(); }

  base::RunLoop run_loop_;

  DISALLOW_COPY_AND_ASSIGN(WallpaperShownWaiter);
};

}  // namespace

class ShelfLayoutManagerTest : public AshTestBase {
 public:
  ShelfLayoutManagerTest() = default;

  // Calls the private SetState() function.
  void SetState(ShelfLayoutManager* layout_manager,
                ShelfVisibilityState state) {
    layout_manager->SetState(state);
  }

  void UpdateAutoHideStateNow() {
    GetShelfLayoutManager()->UpdateAutoHideStateNow();
  }

  aura::Window* CreateTestWindow() {
    aura::Window* window = window_factory::NewWindow().release();
    window->SetProperty(aura::client::kShowStateKey, ui::SHOW_STATE_NORMAL);
    window->SetType(aura::client::WINDOW_TYPE_NORMAL);
    window->Init(ui::LAYER_TEXTURED);
    ParentWindowInPrimaryRootWindow(window);
    return window;
  }

  aura::Window* CreateTestWindowInParent(aura::Window* root_window) {
    aura::Window* window = window_factory::NewWindow().release();
    window->SetProperty(aura::client::kShowStateKey, ui::SHOW_STATE_NORMAL);
    window->SetType(aura::client::WINDOW_TYPE_NORMAL);
    window->Init(ui::LAYER_TEXTURED);
    aura::client::ParentWindowWithContext(window, root_window, gfx::Rect());
    return window;
  }

  // Create a simple widget in the current context (will delete on TearDown).
  views::Widget* CreateTestWidget() {
    views::Widget::InitParams params(views::Widget::InitParams::TYPE_WINDOW);
    params.bounds = gfx::Rect(0, 0, 200, 200);
    params.context = CurrentContext();
    views::Widget* widget = new views::Widget;
    widget->Init(params);
    widget->Show();
    return widget;
  }

  void RunGestureDragTests(const gfx::Point& shown, const gfx::Point& hidden);
  void TestHomeLauncherGestureHandler(bool autohide_shelf);

  gfx::Rect GetVisibleShelfWidgetBoundsInScreen() {
    gfx::Rect bounds = GetShelfWidget()->GetWindowBoundsInScreen();
    bounds.Intersect(
        display::Screen::GetScreen()->GetPrimaryDisplay().bounds());
    return bounds;
  }

  // Turn on the lock screen.
  void LockScreen() { GetSessionControllerClient()->LockScreen(); }

  // Turn off the lock screen.
  void UnlockScreen() { GetSessionControllerClient()->UnlockScreen(); }

  int64_t GetPrimaryDisplayId() {
    return display::Screen::GetScreen()->GetPrimaryDisplay().id();
  }

  void StartScroll(gfx::Point start) {
    timestamp_ = base::TimeTicks::Now();
    current_point_ = start;
    ui::GestureEvent event = ui::GestureEvent(
        current_point_.x(), current_point_.y(), ui::EF_NONE, timestamp_,
        ui::GestureEventDetails(ui::ET_GESTURE_SCROLL_BEGIN, 0, -1.0f));
    GetShelfLayoutManager()->ProcessGestureEvent(event);
  }

  void UpdateScroll(float delta_y) {
    IncreaseTimestamp();
    current_point_.set_y(current_point_.y() + delta_y);
    ui::GestureEvent event = ui::GestureEvent(
        current_point_.x(), current_point_.y(), ui::EF_NONE, timestamp_,
        ui::GestureEventDetails(ui::ET_GESTURE_SCROLL_UPDATE, 0, delta_y));
    GetShelfLayoutManager()->ProcessGestureEvent(event);
  }

  void EndScroll(bool is_fling, float velocity_y) {
    IncreaseTimestamp();
    ui::GestureEventDetails event_details =
        is_fling
            ? ui::GestureEventDetails(ui::ET_SCROLL_FLING_START, 0, velocity_y)
            : ui::GestureEventDetails(ui::ET_GESTURE_SCROLL_END);
    ui::GestureEvent event =
        ui::GestureEvent(current_point_.x(), current_point_.y(), ui::EF_NONE,
                         timestamp_, event_details);
    GetShelfLayoutManager()->ProcessGestureEvent(event);
  }

  void IncreaseTimestamp() {
    timestamp_ += base::TimeDelta::FromMilliseconds(25);
  }

  WorkspaceWindowState GetWorkspaceWindowState() const {
    // Shelf window does not belong to any desk, use the root to get the active
    // desk's workspace state.
    auto* shelf_window = GetShelfWidget()->GetNativeWindow();
    auto* controller =
        GetActiveWorkspaceController(shelf_window->GetRootWindow());
    DCHECK(controller);

    return controller->GetWindowState();
  }

  const ui::Layer* GetNonLockScreenContainersContainerLayer() const {
    const auto* shelf_window = GetShelfWidget()->GetNativeWindow();
    return shelf_window->GetRootWindow()
        ->GetChildById(kShellWindowId_NonLockScreenContainersContainer)
        ->layer();
  }

  // If |layout_manager->auto_hide_timer_| is running, stops it, runs its task,
  // and returns true. Otherwise, returns false.
  bool TriggerAutoHideTimeout() const {
    ShelfLayoutManager* layout_manager = GetShelfLayoutManager();
    if (!layout_manager->auto_hide_timer_.IsRunning())
      return false;

    layout_manager->auto_hide_timer_.FireNow();
    return true;
  }

  // Performs a swipe up gesture to show an auto-hidden shelf.
  void SwipeUpToShowShelf() {
    gfx::Rect display_bounds =
        display::Screen::GetScreen()->GetPrimaryDisplay().bounds();
    const gfx::Point start(display_bounds.bottom_center());
    const gfx::Point end(start + gfx::Vector2d(0, -80));
    const base::TimeDelta kTimeDelta = base::TimeDelta::FromMilliseconds(100);
    const int kNumScrollSteps = 4;
    GetEventGenerator()->GestureScrollSequence(start, end, kTimeDelta,
                                               kNumScrollSteps);
  }

  // Drag Shelf from |start| to |target| by mouse.
  void MouseDragShelfTo(const gfx::Point& start, const gfx::Point& target) {
    ui::test::EventGenerator* generator = GetEventGenerator();
    generator->MoveMouseTo(start);
    generator->PressLeftButton();
    generator->DragMouseTo(target);
    generator->ReleaseLeftButton();
  }

  // Move mouse to show Shelf in auto-hide mode.
  void MouseMouseToShowAutoHiddenShelf() {
    display::Display display =
        display::Screen::GetScreen()->GetPrimaryDisplay();
    const int display_bottom = display.bounds().bottom();
    GetEventGenerator()->MoveMouseTo(1, display_bottom - 1);
    ASSERT_TRUE(TriggerAutoHideTimeout());
    EXPECT_EQ(SHELF_AUTO_HIDE_SHOWN, GetPrimaryShelf()->GetAutoHideState());
  }

 private:
  base::TimeTicks timestamp_;
  gfx::Point current_point_;

  DISALLOW_COPY_AND_ASSIGN(ShelfLayoutManagerTest);
};

void ShelfLayoutManagerTest::RunGestureDragTests(
    const gfx::Point& edge_to_hide,
    const gfx::Point& edge_to_show) {
  ui::test::EventGenerator* generator = GetEventGenerator();
  display::Display display = display::Screen::GetScreen()->GetPrimaryDisplay();
  generator->MoveMouseTo(display.bounds().CenterPoint());

  Shelf* shelf = GetPrimaryShelf();
  shelf->SetAutoHideBehavior(SHELF_AUTO_HIDE_BEHAVIOR_NEVER);

  views::Widget* widget = CreateTestWidget();
  widget->Maximize();

  // The time delta should be large enough to prevent accidental fling creation.
  const base::TimeDelta kTimeDelta = base::TimeDelta::FromMilliseconds(100);

  aura::Window* window = widget->GetNativeWindow();
  ShelfLayoutManager* layout_manager = GetShelfLayoutManager();
  layout_manager->LayoutShelf();

  gfx::Rect shelf_shown = GetShelfWidget()->GetWindowBoundsInScreen();
  gfx::Rect window_bounds_with_shelf = window->bounds();
  EXPECT_EQ(SHELF_VISIBLE, shelf->GetVisibilityState());

  shelf->SetAutoHideBehavior(SHELF_AUTO_HIDE_BEHAVIOR_ALWAYS);
  layout_manager->LayoutShelf();
  EXPECT_EQ(SHELF_AUTO_HIDE_HIDDEN, shelf->GetAutoHideState());

  gfx::Rect window_bounds_with_noshelf = window->bounds();
  gfx::Rect shelf_hidden = GetShelfWidget()->GetWindowBoundsInScreen();

  // Tests the gesture drag on always shown shelf.
  shelf->SetAutoHideBehavior(SHELF_AUTO_HIDE_BEHAVIOR_NEVER);
  layout_manager->LayoutShelf();

  const int kNumScrollSteps = 4;
  ShelfDragCallback handler(shelf_hidden, shelf_shown);

  // Swipe down on the always shown shelf should not auto-hide it.
  {
    SCOPED_TRACE("SWIPE_DOWN_ALWAYS_SHOWN");
    generator->GestureScrollSequenceWithCallback(
        edge_to_hide, edge_to_show, kTimeDelta, kNumScrollSteps,
        base::BindRepeating(&ShelfDragCallback::ProcessScroll,
                            base::Unretained(&handler)));
  }
  EXPECT_EQ(SHELF_VISIBLE, shelf->GetVisibilityState());
  EXPECT_EQ(SHELF_AUTO_HIDE_BEHAVIOR_NEVER, shelf->auto_hide_behavior());
  EXPECT_EQ(window_bounds_with_shelf.ToString(), window->bounds().ToString());
  EXPECT_EQ(shelf_shown.ToString(),
            GetShelfWidget()->GetWindowBoundsInScreen().ToString());

  // Verify that the shelf can still enter auto hide if the |widget_| has been
  // put into fullscreen.
  widget->SetFullscreen(true);
  WindowState* window_state = WindowState::Get(window);
  window_state->SetHideShelfWhenFullscreen(false);
  window->SetProperty(kImmersiveIsActive, true);
  layout_manager->UpdateVisibilityState();
  EXPECT_EQ(SHELF_AUTO_HIDE, shelf->GetVisibilityState());
  EXPECT_EQ(SHELF_AUTO_HIDE_HIDDEN, shelf->GetAutoHideState());
  EXPECT_EQ(SHELF_AUTO_HIDE_BEHAVIOR_NEVER, shelf->auto_hide_behavior());

  // Swiping up should show the shelf if shelf is hidden in fullscreen mode.
  generator->GestureScrollSequence(edge_to_hide, edge_to_show, kTimeDelta,
                                   kNumScrollSteps);
  EXPECT_EQ(SHELF_AUTO_HIDE, shelf->GetVisibilityState());
  EXPECT_EQ(SHELF_AUTO_HIDE_SHOWN, shelf->GetAutoHideState());
  EXPECT_EQ(SHELF_AUTO_HIDE_BEHAVIOR_NEVER, shelf->auto_hide_behavior());

  // Swiping down should hide the shelf.
  generator->GestureScrollSequence(edge_to_show, edge_to_hide, kTimeDelta,
                                   kNumScrollSteps);
  EXPECT_EQ(SHELF_AUTO_HIDE, shelf->GetVisibilityState());
  EXPECT_EQ(SHELF_AUTO_HIDE_HIDDEN, shelf->GetAutoHideState());
  EXPECT_EQ(SHELF_AUTO_HIDE_BEHAVIOR_NEVER, shelf->auto_hide_behavior());

  // Verify that after toggling fullscreen to off, the shelf is visible.
  widget->SetFullscreen(false);
  EXPECT_EQ(SHELF_VISIBLE, shelf->GetVisibilityState());

  // Minimize the visible window, the shelf should be shown if there are no
  // visible windows, even in auto-hide mode.
  window_state->Minimize();
  EXPECT_EQ(SHELF_VISIBLE, shelf->GetVisibilityState());
  EXPECT_EQ(SHELF_AUTO_HIDE_BEHAVIOR_NEVER, shelf->auto_hide_behavior());

  // Tests gesture drag on auto-hide shelf.
  window_state->Maximize();
  shelf->SetAutoHideBehavior(SHELF_AUTO_HIDE_BEHAVIOR_ALWAYS);
  layout_manager->LayoutShelf();
  EXPECT_EQ(SHELF_AUTO_HIDE_HIDDEN, shelf->GetAutoHideState());

  // Swipe up the auto-hide shelf should show it.
  {
    SCOPED_TRACE("SWIPE_UP_AUTO_HIDE_SHOW");
    generator->GestureScrollSequenceWithCallback(
        edge_to_hide, edge_to_show, kTimeDelta, kNumScrollSteps,
        base::BindRepeating(&ShelfDragCallback::ProcessScroll,
                            base::Unretained(&handler)));
  }
  EXPECT_EQ(SHELF_AUTO_HIDE, shelf->GetVisibilityState());
  EXPECT_EQ(SHELF_AUTO_HIDE_SHOWN, shelf->GetAutoHideState());
  // Gesture drag should not change the auto hide behavior of shelf, even though
  // its visibility has been changed.
  EXPECT_EQ(SHELF_AUTO_HIDE_BEHAVIOR_ALWAYS, shelf->auto_hide_behavior());
  // The auto-hide shelf is above the window, which should not change the bounds
  // of the window.
  EXPECT_EQ(window_bounds_with_noshelf.ToString(), window->bounds().ToString());
  EXPECT_EQ(shelf_shown.ToString(),
            GetShelfWidget()->GetWindowBoundsInScreen().ToString());

  // Swipe down very little. It shouldn't change any state.
  gfx::Point new_point(edge_to_show);
  gfx::Vector2d diff = edge_to_hide - edge_to_show;
  new_point.Offset(diff.x() * 3 / 10, diff.y() * 3 / 10);
  generator->GestureScrollSequence(edge_to_show, new_point, kTimeDelta, 5);
  EXPECT_EQ(SHELF_AUTO_HIDE, shelf->GetVisibilityState());
  EXPECT_EQ(SHELF_AUTO_HIDE_SHOWN, shelf->GetAutoHideState());
  EXPECT_EQ(SHELF_AUTO_HIDE_BEHAVIOR_ALWAYS, shelf->auto_hide_behavior());
  EXPECT_EQ(window_bounds_with_noshelf.ToString(), window->bounds().ToString());
  EXPECT_EQ(shelf_shown.ToString(),
            GetShelfWidget()->GetWindowBoundsInScreen().ToString());

  {
    SCOPED_TRACE("SWIPE_DOWN_AUTO_HIDE_1");
    generator->GestureScrollSequenceWithCallback(
        edge_to_show, edge_to_hide, kTimeDelta, kNumScrollSteps,
        base::BindRepeating(&ShelfDragCallback::ProcessScroll,
                            base::Unretained(&handler)));
  }
  EXPECT_EQ(SHELF_AUTO_HIDE, shelf->GetVisibilityState());
  EXPECT_EQ(SHELF_AUTO_HIDE_HIDDEN, shelf->GetAutoHideState());
  EXPECT_EQ(SHELF_AUTO_HIDE_BEHAVIOR_ALWAYS, shelf->auto_hide_behavior());
  EXPECT_EQ(window_bounds_with_noshelf.ToString(), window->bounds().ToString());
  EXPECT_EQ(shelf_hidden.ToString(),
            GetShelfWidget()->GetWindowBoundsInScreen().ToString());

  // Swipe up in extended hit region to show it.
  gfx::Point extended_start = edge_to_show;
  if (shelf->IsHorizontalAlignment())
    extended_start.set_y(GetShelfWidget()->GetWindowBoundsInScreen().y() - 1);
  else if (SHELF_ALIGNMENT_LEFT == shelf->alignment())
    extended_start.set_x(GetShelfWidget()->GetWindowBoundsInScreen().right() +
                         1);
  else if (SHELF_ALIGNMENT_RIGHT == shelf->alignment())
    extended_start.set_x(GetShelfWidget()->GetWindowBoundsInScreen().x() - 1);
  {
    SCOPED_TRACE("SWIPE_UP_EXTENDED_HIT");
    generator->GestureScrollSequenceWithCallback(
        extended_start, edge_to_show, kTimeDelta, kNumScrollSteps,
        base::BindRepeating(&ShelfDragCallback::ProcessScroll,
                            base::Unretained(&handler)));
  }

  EXPECT_EQ(SHELF_AUTO_HIDE, shelf->GetVisibilityState());
  EXPECT_EQ(SHELF_AUTO_HIDE_SHOWN, shelf->GetAutoHideState());
  EXPECT_EQ(SHELF_AUTO_HIDE_BEHAVIOR_ALWAYS, shelf->auto_hide_behavior());
  EXPECT_EQ(window_bounds_with_noshelf.ToString(), window->bounds().ToString());
  EXPECT_EQ(shelf_shown.ToString(),
            GetShelfWidget()->GetWindowBoundsInScreen().ToString());

  // Swipe down again to hide.
  {
    SCOPED_TRACE("SWIPE_DOWN_AUTO_HIDE_2");
    generator->GestureScrollSequenceWithCallback(
        edge_to_show, edge_to_hide, kTimeDelta, kNumScrollSteps,
        base::BindRepeating(&ShelfDragCallback::ProcessScroll,
                            base::Unretained(&handler)));
  }
  EXPECT_EQ(SHELF_AUTO_HIDE, shelf->GetVisibilityState());
  EXPECT_EQ(SHELF_AUTO_HIDE_HIDDEN, shelf->GetAutoHideState());
  EXPECT_EQ(SHELF_AUTO_HIDE_BEHAVIOR_ALWAYS, shelf->auto_hide_behavior());
  EXPECT_EQ(window_bounds_with_noshelf.ToString(), window->bounds().ToString());
  EXPECT_EQ(shelf_hidden.ToString(),
            GetShelfWidget()->GetWindowBoundsInScreen().ToString());

  // Swipe up outside the hit area. This should not change anything.
  gfx::Point outside_start =
      GetShelfWidget()->GetWindowBoundsInScreen().top_center();
  outside_start.set_y(outside_start.y() - 50);
  gfx::Vector2d delta = edge_to_hide - edge_to_show;
  generator->GestureScrollSequence(outside_start, outside_start + delta,
                                   kTimeDelta, kNumScrollSteps);
  EXPECT_EQ(SHELF_AUTO_HIDE, shelf->GetVisibilityState());
  EXPECT_EQ(SHELF_AUTO_HIDE_HIDDEN, shelf->GetAutoHideState());
  EXPECT_EQ(SHELF_AUTO_HIDE_BEHAVIOR_ALWAYS, shelf->auto_hide_behavior());
  EXPECT_EQ(window_bounds_with_noshelf.ToString(), window->bounds().ToString());
  EXPECT_EQ(shelf_hidden.ToString(),
            GetShelfWidget()->GetWindowBoundsInScreen().ToString());
  // Swipe up from the bottom of the shelf, this should show the shelf.
  gfx::Point below_start = edge_to_hide;
  generator->GestureScrollSequence(edge_to_hide, edge_to_show, kTimeDelta,
                                   kNumScrollSteps);
  EXPECT_EQ(SHELF_AUTO_HIDE, shelf->GetVisibilityState());
  EXPECT_EQ(SHELF_AUTO_HIDE_SHOWN, shelf->GetAutoHideState());
  EXPECT_EQ(SHELF_AUTO_HIDE_BEHAVIOR_ALWAYS, shelf->auto_hide_behavior());
  EXPECT_EQ(window_bounds_with_noshelf.ToString(), window->bounds().ToString());
  EXPECT_EQ(shelf_shown.ToString(),
            GetShelfWidget()->GetWindowBoundsInScreen().ToString());

  // Swipe down again to hide.
  {
    SCOPED_TRACE("SWIPE_DOWN_AUTO_HIDE_3");
    generator->GestureScrollSequenceWithCallback(
        edge_to_show, edge_to_hide, kTimeDelta, kNumScrollSteps,
        base::BindRepeating(&ShelfDragCallback::ProcessScroll,
                            base::Unretained(&handler)));
  }
  EXPECT_EQ(SHELF_AUTO_HIDE, shelf->GetVisibilityState());
  EXPECT_EQ(SHELF_AUTO_HIDE_HIDDEN, shelf->GetAutoHideState());
  EXPECT_EQ(SHELF_AUTO_HIDE_BEHAVIOR_ALWAYS, shelf->auto_hide_behavior());
  EXPECT_EQ(window_bounds_with_noshelf.ToString(), window->bounds().ToString());
  EXPECT_EQ(shelf_hidden.ToString(),
            GetShelfWidget()->GetWindowBoundsInScreen().ToString());

  // Put |widget| into fullscreen. Set the shelf to be auto hidden when |widget|
  // is fullscreen. (eg browser immersive fullscreen).
  widget->SetFullscreen(true);
  WindowState::Get(window)->SetHideShelfWhenFullscreen(false);
  layout_manager->UpdateVisibilityState();

  gfx::Rect window_bounds_fullscreen = window->bounds();
  EXPECT_TRUE(widget->IsFullscreen());

  EXPECT_EQ(window_bounds_with_noshelf.ToString(),
            window_bounds_fullscreen.ToString());

  // Swipe up. This should show the shelf.
  {
    SCOPED_TRACE("SWIPE_UP_AUTO_HIDE_1");
    // Do not check bounds because the events outside of the bounds
    // will be clipped.
    generator->GestureScrollSequenceWithCallback(
        below_start, edge_to_show, kTimeDelta, kNumScrollSteps,
        base::BindRepeating(&ShelfDragCallback::ProcessScrollNoBoundsCheck,
                            base::Unretained(&handler)));
  }
  EXPECT_EQ(SHELF_AUTO_HIDE, shelf->GetVisibilityState());
  EXPECT_EQ(SHELF_AUTO_HIDE_SHOWN, shelf->GetAutoHideState());
  EXPECT_EQ(SHELF_AUTO_HIDE_BEHAVIOR_ALWAYS, shelf->auto_hide_behavior());
  EXPECT_EQ(shelf_shown.ToString(),
            GetShelfWidget()->GetWindowBoundsInScreen().ToString());
  EXPECT_EQ(window_bounds_fullscreen.ToString(), window->bounds().ToString());

  // Swipe down to hide the shelf.
  {
    SCOPED_TRACE("SWIPE_DOWN_AUTO_HIDE_4");
    generator->GestureScrollSequenceWithCallback(
        edge_to_show, edge_to_hide, kTimeDelta, kNumScrollSteps,
        base::BindRepeating(&ShelfDragCallback::ProcessScroll,
                            base::Unretained(&handler)));
  }
  EXPECT_EQ(SHELF_AUTO_HIDE, shelf->GetVisibilityState());
  EXPECT_EQ(SHELF_AUTO_HIDE_HIDDEN, shelf->GetAutoHideState());
  EXPECT_EQ(SHELF_AUTO_HIDE_BEHAVIOR_ALWAYS, shelf->auto_hide_behavior());
  EXPECT_EQ(shelf_hidden.ToString(),
            GetShelfWidget()->GetWindowBoundsInScreen().ToString());
  EXPECT_EQ(window_bounds_fullscreen.ToString(), window->bounds().ToString());

  // Set the shelf to be hidden when |widget| is fullscreen. (eg tab fullscreen
  // with or without immersive browser fullscreen).
  WindowState::Get(window)->SetHideShelfWhenFullscreen(true);

  layout_manager->UpdateVisibilityState();
  EXPECT_EQ(SHELF_HIDDEN, shelf->GetVisibilityState());
  EXPECT_EQ(SHELF_AUTO_HIDE_BEHAVIOR_ALWAYS, shelf->auto_hide_behavior());

  // Swipe-up. This should not change anything.
  {
    SCOPED_TRACE("SWIPE_UP_NO_CHANGE");
    generator->GestureScrollSequenceWithCallback(
        below_start, edge_to_show, kTimeDelta, kNumScrollSteps,
        base::BindRepeating(&ShelfDragCallback::ProcessScroll,
                            base::Unretained(&handler)));
    EXPECT_EQ(SHELF_HIDDEN, shelf->GetVisibilityState());
    EXPECT_EQ(SHELF_AUTO_HIDE_BEHAVIOR_ALWAYS, shelf->auto_hide_behavior());
    EXPECT_EQ(window_bounds_fullscreen.ToString(), window->bounds().ToString());
  }

  // Minimize actually, otherwise further event may be affected since widget
  // is fullscreen status.
  widget->Minimize();
  base::RunLoop().RunUntilIdle();
  EXPECT_FALSE(layout_manager->HasVisibleWindow());

  // The shelf should be shown because there are no more visible windows.
  EXPECT_EQ(SHELF_AUTO_HIDE, shelf->GetVisibilityState());
  EXPECT_EQ(SHELF_AUTO_HIDE_SHOWN, shelf->GetAutoHideState());
  EXPECT_EQ(SHELF_AUTO_HIDE_BEHAVIOR_ALWAYS, shelf->auto_hide_behavior());

  // Swipe-down to hide. This should have no effect because there are no visible
  // windows.
  {
    SCOPED_TRACE("SWIPE_DOWN_AUTO_HIDE_5");
    generator->GestureScrollSequenceWithCallback(
        edge_to_show, edge_to_hide, kTimeDelta, kNumScrollSteps,
        base::BindRepeating(&ShelfDragCallback::ProcessScroll,
                            base::Unretained(&handler)));
  }
  EXPECT_EQ(SHELF_AUTO_HIDE, shelf->GetVisibilityState());
  EXPECT_EQ(SHELF_AUTO_HIDE_SHOWN, shelf->GetAutoHideState());
  EXPECT_EQ(SHELF_AUTO_HIDE_BEHAVIOR_ALWAYS, shelf->auto_hide_behavior());
  EXPECT_EQ(shelf_shown.ToString(),
            GetShelfWidget()->GetWindowBoundsInScreen().ToString());

  widget->Restore();
  base::RunLoop().RunUntilIdle();
  EXPECT_TRUE(layout_manager->HasVisibleWindow());

  // Swipe up on the shelf. This should show the shelf but should not change the
  // auto-hide behavior, since auto-hide behavior can only be changed through
  // context menu of the shelf.
  {
    SCOPED_TRACE("SWIPE_UP_AUTO_HIDE_2");
    // Do not check bounds because the events outside of the bounds
    // will be clipped.
    generator->GestureScrollSequenceWithCallback(
        below_start, edge_to_show, kTimeDelta, kNumScrollSteps,
        base::BindRepeating(&ShelfDragCallback::ProcessScrollNoBoundsCheck,
                            base::Unretained(&handler)));
  }
  EXPECT_EQ(SHELF_AUTO_HIDE_SHOWN, shelf->GetAutoHideState());
  EXPECT_EQ(SHELF_AUTO_HIDE_BEHAVIOR_ALWAYS, shelf->auto_hide_behavior());
  EXPECT_EQ(shelf_shown.ToString(),
            GetShelfWidget()->GetWindowBoundsInScreen().ToString());

  widget->Close();
  base::RunLoop().RunUntilIdle();
  EXPECT_FALSE(layout_manager->HasVisibleWindow());

  // Swipe-down to hide. This should have no effect because there are no visible
  // windows.
  {
    SCOPED_TRACE("SWIPE_DOWN_AUTO_HIDE_6");
    generator->GestureScrollSequenceWithCallback(
        edge_to_show, edge_to_hide, kTimeDelta, kNumScrollSteps,
        base::BindRepeating(&ShelfDragCallback::ProcessScroll,
                            base::Unretained(&handler)));
  }
  EXPECT_EQ(SHELF_AUTO_HIDE_SHOWN, shelf->GetAutoHideState());
  EXPECT_EQ(SHELF_AUTO_HIDE_BEHAVIOR_ALWAYS, shelf->auto_hide_behavior());
  EXPECT_EQ(shelf_shown.ToString(),
            GetShelfWidget()->GetWindowBoundsInScreen().ToString());

  // Swipe up again on AUTO_HIDE_SHOWN shelf shouldn't change any state.
  // Swipe up on auto-hide shown shelf should still keep shelf shown.
  {
    SCOPED_TRACE("SWIPE_UP_AUTO_HIDE_4");
    generator->GestureScrollSequenceWithCallback(
        edge_to_hide, edge_to_show, kTimeDelta, kNumScrollSteps,
        base::BindRepeating(&ShelfDragCallback::ProcessScroll,
                            base::Unretained(&handler)));
  }
  EXPECT_EQ(SHELF_AUTO_HIDE, shelf->GetVisibilityState());
  EXPECT_EQ(SHELF_AUTO_HIDE_SHOWN, shelf->GetAutoHideState());
  EXPECT_EQ(shelf_shown.ToString(),
            GetShelfWidget()->GetWindowBoundsInScreen().ToString());
}

void ShelfLayoutManagerTest::TestHomeLauncherGestureHandler(
    bool autohide_shelf) {
  // Home launcher is only available in tablet mode.
  TabletModeControllerTestApi().EnterTabletMode();

  Shelf* shelf = GetPrimaryShelf();
  if (autohide_shelf)
    shelf->SetAutoHideBehavior(SHELF_AUTO_HIDE_BEHAVIOR_ALWAYS);

  // Create more than one window to prepare for the possibly window stacking
  // change during drag.
  std::unique_ptr<aura::Window> extra_window =
      AshTestBase::CreateTestWindow(gfx::Rect(100, 10, 100, 100));
  std::unique_ptr<aura::Window> window =
      AshTestBase::CreateTestWindow(gfx::Rect(0, 0, 400, 400));
  wm::ActivateWindow(window.get());

  if (autohide_shelf) {
    SwipeUpToShowShelf();
    EXPECT_EQ(SHELF_AUTO_HIDE_SHOWN, shelf->GetAutoHideState());
  }

  const gfx::Point shelf_center =
      GetVisibleShelfWidgetBoundsInScreen().CenterPoint();

  // Helper to create a scroll event for this test.
  auto create_scroll_event = [&shelf_center](ui::EventType type,
                                             float scroll_y) {
    ui::GestureEventDetails details =
        type == ui::ET_GESTURE_SCROLL_END
            ? ui::GestureEventDetails(type)
            : ui::GestureEventDetails(type, 0, scroll_y);
    return ui::GestureEvent(shelf_center.x(), shelf_center.y(), 0,
                            base::TimeTicks(), details);
  };

  // The home launcher gesture handler should not be handling any window
  // initially.
  HomeLauncherGestureHandler* gesture_handler =
      Shell::Get()->home_screen_controller()->home_launcher_gesture_handler();
  ASSERT_TRUE(gesture_handler);
  ASSERT_FALSE(gesture_handler->GetActiveWindow());

  // Tests that after scrolling up on the shelf, the home launcher gesture
  // handler will be acting on |window|, and the shelf becomes transparent.
  ShelfLayoutManager* manager = GetShelfLayoutManager();
  manager->ProcessGestureEvent(
      create_scroll_event(ui::ET_GESTURE_SCROLL_BEGIN, -1.f));
  EXPECT_EQ(window.get(), gesture_handler->GetActiveWindow());
  EXPECT_EQ(SHELF_BACKGROUND_DEFAULT, GetShelfWidget()->GetBackgroundType());
  if (autohide_shelf) {
    // Auto-hide shelf should keep visible after scrolling up on it.
    EXPECT_EQ(SHELF_AUTO_HIDE_SHOWN, shelf->GetAutoHideState());
  }

  // Tests that since the initial scroll event was scrolled up, the home
  // launcher gesture handler will continue to act on |window| regardless of
  // direction of scroll updates.
  manager->ProcessGestureEvent(
      create_scroll_event(ui::ET_GESTURE_SCROLL_UPDATE, -1.f));
  EXPECT_EQ(window.get(), gesture_handler->GetActiveWindow());
  EXPECT_EQ(SHELF_BACKGROUND_DEFAULT, GetShelfWidget()->GetBackgroundType());

  manager->ProcessGestureEvent(
      create_scroll_event(ui::ET_GESTURE_SCROLL_UPDATE, 1.f));
  EXPECT_EQ(window.get(), gesture_handler->GetActiveWindow());
  EXPECT_EQ(SHELF_BACKGROUND_DEFAULT, GetShelfWidget()->GetBackgroundType());
  if (autohide_shelf)
    EXPECT_EQ(SHELF_AUTO_HIDE_SHOWN, shelf->GetAutoHideState());

  // End the scroll.
  manager->ProcessGestureEvent(
      create_scroll_event(ui::ET_GESTURE_SCROLL_END, 1.f));
  ASSERT_FALSE(gesture_handler->GetActiveWindow());
  if (autohide_shelf)
    EXPECT_EQ(SHELF_AUTO_HIDE_SHOWN, shelf->GetAutoHideState());

  // Tests that if the initial scroll event is directed downwards, the home
  // launcher gesture handler will not act on |window|.
  manager->ProcessGestureEvent(
      create_scroll_event(ui::ET_GESTURE_SCROLL_BEGIN, 1.f));
  EXPECT_FALSE(gesture_handler->GetActiveWindow());
  manager->ProcessGestureEvent(
      create_scroll_event(ui::ET_GESTURE_SCROLL_UPDATE, -1.f));
  EXPECT_FALSE(gesture_handler->GetActiveWindow());
}

// Makes sure SetVisible updates work area and widget appropriately.
TEST_F(ShelfLayoutManagerTest, SetVisible) {
  ShelfWidget* shelf_widget = GetShelfWidget();
  ShelfLayoutManager* manager = shelf_widget->shelf_layout_manager();
  // Force an initial layout.
  manager->LayoutShelf();
  EXPECT_EQ(SHELF_VISIBLE, manager->visibility_state());

  gfx::Rect status_bounds(
      shelf_widget->status_area_widget()->GetWindowBoundsInScreen());
  gfx::Rect shelf_bounds(shelf_widget->GetWindowBoundsInScreen());
  int shelf_height = manager->GetIdealBounds().height();
  display::Display display = display::Screen::GetScreen()->GetPrimaryDisplay();
  const gfx::Rect stable_work_area = display.work_area();
  ASSERT_NE(-1, display.id());
  // Bottom inset should be the max of widget heights.
  EXPECT_EQ(shelf_height, display.GetWorkAreaInsets().bottom());

  // Hide the shelf.
  SetState(manager, SHELF_HIDDEN);
  // Run the animation to completion.
  StepWidgetLayerAnimatorToEnd(shelf_widget);
  StepWidgetLayerAnimatorToEnd(shelf_widget->status_area_widget());
  EXPECT_EQ(SHELF_HIDDEN, manager->visibility_state());
  display = display::Screen::GetScreen()->GetPrimaryDisplay();
  EXPECT_EQ(0, display.GetWorkAreaInsets().bottom());

  // Make sure the bounds of the two widgets changed.
  EXPECT_GE(shelf_widget->GetNativeView()->bounds().y(),
            display.bounds().bottom());
  EXPECT_GE(shelf_widget->status_area_widget()->GetNativeView()->bounds().y(),
            display.bounds().bottom());
  EXPECT_EQ(stable_work_area,
            GetPrimaryWorkAreaInsets()->ComputeStableWorkArea());

  // And show it again.
  SetState(manager, SHELF_VISIBLE);
  // Run the animation to completion.
  StepWidgetLayerAnimatorToEnd(shelf_widget);
  StepWidgetLayerAnimatorToEnd(shelf_widget->status_area_widget());
  EXPECT_EQ(SHELF_VISIBLE, manager->visibility_state());
  display = display::Screen::GetScreen()->GetPrimaryDisplay();
  EXPECT_EQ(shelf_height, display.GetWorkAreaInsets().bottom());
  EXPECT_EQ(stable_work_area,
            GetPrimaryWorkAreaInsets()->ComputeStableWorkArea());

  // Make sure the bounds of the two widgets changed.
  shelf_bounds = shelf_widget->GetNativeView()->bounds();
  EXPECT_LT(shelf_bounds.y(), display.bounds().bottom());
  status_bounds = shelf_widget->status_area_widget()->GetNativeView()->bounds();
  EXPECT_LT(status_bounds.y(), display.bounds().bottom());
}

// Makes sure LayoutShelf invoked while animating cleans things up.
TEST_F(ShelfLayoutManagerTest, LayoutShelfWhileAnimating) {
  Shelf* shelf = GetPrimaryShelf();
  ShelfLayoutManager* layout_manager = GetShelfLayoutManager();
  // Force an initial layout.
  layout_manager->LayoutShelf();
  EXPECT_EQ(SHELF_VISIBLE, shelf->GetVisibilityState());

  // Hide the shelf.
  SetState(layout_manager, SHELF_HIDDEN);
  layout_manager->LayoutShelf();
  EXPECT_EQ(SHELF_HIDDEN, shelf->GetVisibilityState());
  display::Display display = display::Screen::GetScreen()->GetPrimaryDisplay();
  EXPECT_EQ(0, display.GetWorkAreaInsets().bottom());

  // Make sure the bounds of the two widgets changed.
  ShelfWidget* shelf_widget = GetShelfWidget();
  EXPECT_GE(shelf_widget->GetNativeView()->bounds().y(),
            display.bounds().bottom());
  EXPECT_GE(shelf_widget->status_area_widget()->GetNativeView()->bounds().y(),
            display.bounds().bottom());
}

// Test that switching to a different visibility state does not restart the
// shelf show / hide animation if it is already running. (crbug.com/250918)
TEST_F(ShelfLayoutManagerTest, SetStateWhileAnimating) {
  ShelfLayoutManager* layout_manager = GetShelfLayoutManager();
  SetState(layout_manager, SHELF_VISIBLE);
  ShelfWidget* shelf_widget = GetShelfWidget();
  gfx::Rect initial_shelf_bounds = shelf_widget->GetWindowBoundsInScreen();
  gfx::Rect initial_status_bounds =
      shelf_widget->status_area_widget()->GetWindowBoundsInScreen();

  ui::ScopedAnimationDurationScaleMode normal_animation_duration(
      ui::ScopedAnimationDurationScaleMode::SLOW_DURATION);
  SetState(layout_manager, SHELF_HIDDEN);
  SetState(layout_manager, SHELF_VISIBLE);

  gfx::Rect current_shelf_bounds = shelf_widget->GetWindowBoundsInScreen();
  gfx::Rect current_status_bounds =
      shelf_widget->status_area_widget()->GetWindowBoundsInScreen();

  const int small_change = initial_shelf_bounds.height() / 2;
  EXPECT_LE(
      std::abs(initial_shelf_bounds.height() - current_shelf_bounds.height()),
      small_change);
  EXPECT_LE(
      std::abs(initial_status_bounds.height() - current_status_bounds.height()),
      small_change);
}

// Makes sure the shelf is sized when the status area changes size.
TEST_F(ShelfLayoutManagerTest, ShelfUpdatedWhenStatusAreaChangesSize) {
  Shelf* shelf = GetPrimaryShelf();
  ASSERT_TRUE(shelf);
  ShelfWidget* shelf_widget = GetShelfWidget();
  ASSERT_TRUE(shelf_widget);
  ASSERT_TRUE(shelf_widget->status_area_widget());
  shelf_widget->status_area_widget()->SetBounds(gfx::Rect(0, 0, 200, 200));
  EXPECT_EQ(200, shelf_widget->GetContentsView()->width() -
                     shelf->GetShelfViewForTesting()->width());
}

// Various assertions around auto-hide.
TEST_F(ShelfLayoutManagerTest, AutoHide) {
  ui::test::EventGenerator* generator = GetEventGenerator();

  const gfx::Rect stable_work_area =
      display::Screen::GetScreen()->GetPrimaryDisplay().work_area();

  Shelf* shelf = GetPrimaryShelf();
  ShelfLayoutManager* layout_manager = GetShelfLayoutManager();
  shelf->SetAutoHideBehavior(SHELF_AUTO_HIDE_BEHAVIOR_ALWAYS);
  views::Widget* widget = CreateTestWidget();
  widget->Maximize();
  EXPECT_EQ(SHELF_AUTO_HIDE, shelf->GetVisibilityState());
  EXPECT_EQ(SHELF_AUTO_HIDE_HIDDEN, shelf->GetAutoHideState());

  display::Display display = display::Screen::GetScreen()->GetPrimaryDisplay();

  // LayoutShelf() forces the animation to completion, at which point the
  // shelf should go off the screen.
  layout_manager->LayoutShelf();

  const int display_bottom = display.bounds().bottom();
  EXPECT_EQ(display_bottom - kHiddenShelfInScreenPortion,
            GetShelfWidget()->GetWindowBoundsInScreen().y());
  EXPECT_EQ(display_bottom, display.work_area().bottom());
  EXPECT_EQ(stable_work_area,
            GetPrimaryWorkAreaInsets()->ComputeStableWorkArea());

  // Move the mouse to the bottom of the screen.
  generator->MoveMouseTo(0, display_bottom - 1);

  // Shelf should be shown again (but it shouldn't have changed the work area).
  SetState(layout_manager, SHELF_AUTO_HIDE);
  EXPECT_EQ(SHELF_AUTO_HIDE_SHOWN, shelf->GetAutoHideState());
  layout_manager->LayoutShelf();
  EXPECT_EQ(display_bottom - layout_manager->GetIdealBounds().height(),
            GetShelfWidget()->GetWindowBoundsInScreen().y());
  EXPECT_EQ(display_bottom, display.work_area().bottom());
  EXPECT_EQ(stable_work_area,
            GetPrimaryWorkAreaInsets()->ComputeStableWorkArea());

  // Tap the system tray when shelf is shown should open the system tray menu.
  generator->GestureTapAt(
      GetPrimaryUnifiedSystemTray()->GetBoundsInScreen().CenterPoint());
  EXPECT_TRUE(GetPrimaryUnifiedSystemTray()->IsBubbleShown());

  // Move mouse back up and click to dismiss the opened system tray menu.
  generator->MoveMouseTo(0, 0);
  generator->ClickLeftButton();
  SetState(layout_manager, SHELF_AUTO_HIDE);
  EXPECT_EQ(SHELF_AUTO_HIDE_HIDDEN, shelf->GetAutoHideState());
  layout_manager->LayoutShelf();
  EXPECT_EQ(display_bottom - kHiddenShelfInScreenPortion,
            GetShelfWidget()->GetWindowBoundsInScreen().y());
  EXPECT_EQ(stable_work_area,
            GetPrimaryWorkAreaInsets()->ComputeStableWorkArea());

  // Move the mouse to the bottom again to show the shelf.
  generator->MoveMouseTo(0, display_bottom - 1);
  UpdateAutoHideStateNow();
  EXPECT_EQ(SHELF_AUTO_HIDE_SHOWN, shelf->GetAutoHideState());

  // A tap on the maximized window should hide the shelf, even if the most
  // recent mouse position was over the shelf (crbug.com/963977).
  EXPECT_TRUE(widget->IsMouseEventsEnabled());
  gfx::Rect window_bounds = widget->GetNativeWindow()->GetBoundsInScreen();
  generator->GestureTapAt(window_bounds.origin() + gfx::Vector2d(10, 10));
  EXPECT_FALSE(widget->IsMouseEventsEnabled());
  UpdateAutoHideStateNow();
  EXPECT_EQ(SHELF_AUTO_HIDE_HIDDEN, shelf->GetAutoHideState());

  // Return the mouse to the top.
  generator->MoveMouseTo(0, 0);

  // Drag mouse to bottom of screen.
  generator->PressLeftButton();
  generator->MoveMouseTo(0, display_bottom - 1);
  UpdateAutoHideStateNow();
  EXPECT_EQ(SHELF_AUTO_HIDE_HIDDEN, shelf->GetAutoHideState());

  generator->ReleaseLeftButton();
  generator->MoveMouseTo(1, display_bottom - 1);
  UpdateAutoHideStateNow();
  EXPECT_EQ(SHELF_AUTO_HIDE_SHOWN, shelf->GetAutoHideState());
  generator->PressLeftButton();
  generator->MoveMouseTo(1, display_bottom - 1);
  UpdateAutoHideStateNow();
  EXPECT_EQ(SHELF_AUTO_HIDE_SHOWN, shelf->GetAutoHideState());

  // Switch to tablet mode should hide the AUTO_HIDE_SHOWN shelf even the mouse
  // cursor is inside the shelf area.
  EXPECT_FALSE(TabletModeControllerTestApi().IsTabletModeStarted());
  TabletModeControllerTestApi().EnterTabletMode();
  EXPECT_TRUE(TabletModeControllerTestApi().IsTabletModeStarted());
  EXPECT_EQ(SHELF_AUTO_HIDE_HIDDEN, shelf->GetAutoHideState());
}

// Test the behavior of the shelf when it is auto hidden and it is on the
// boundary between the primary and the secondary display.
TEST_F(ShelfLayoutManagerTest, AutoHideShelfOnScreenBoundary) {
  UpdateDisplay("800x600,800x600");
  Shell::Get()->display_manager()->SetLayoutForCurrentDisplays(
      display::test::CreateDisplayLayout(display_manager(),
                                         display::DisplayPlacement::RIGHT, 0));
  // Put the primary monitor's shelf on the display boundary.
  Shelf* shelf = GetPrimaryShelf();
  shelf->SetAlignment(SHELF_ALIGNMENT_RIGHT);

  // Create a window because the shelf is always shown when no windows are
  // visible.
  CreateTestWidget();

  shelf->SetAutoHideBehavior(SHELF_AUTO_HIDE_BEHAVIOR_ALWAYS);
  EXPECT_EQ(SHELF_AUTO_HIDE, shelf->GetVisibilityState());

  display::Display display = display::Screen::GetScreen()->GetPrimaryDisplay();
  const int right_edge = display.bounds().right() - 1;
  const int y = display.bounds().y();

  // Start off the mouse nowhere near the shelf; the shelf should be hidden.
  ui::test::EventGenerator* generator = GetEventGenerator();
  generator->MoveMouseTo(right_edge - 50, y);
  UpdateAutoHideStateNow();
  EXPECT_EQ(SHELF_AUTO_HIDE_HIDDEN, shelf->GetAutoHideState());

  // Moving the mouse over the light bar (but not to the edge of the screen)
  // should show the shelf.
  generator->MoveMouseTo(right_edge - 1, y);
  UpdateAutoHideStateNow();
  EXPECT_EQ(SHELF_AUTO_HIDE_SHOWN, shelf->GetAutoHideState());
  EXPECT_EQ(right_edge - 1,
            display::Screen::GetScreen()->GetCursorScreenPoint().x());

  // Moving the mouse off the light bar should hide the shelf.
  generator->MoveMouseTo(right_edge - 60, y);
  UpdateAutoHideStateNow();
  EXPECT_EQ(SHELF_AUTO_HIDE_HIDDEN, shelf->GetAutoHideState());

  // Moving the mouse to the right edge of the screen crossing the light bar
  // should show the shelf despite the mouse cursor getting warped to the
  // secondary display.
  generator->MoveMouseTo(right_edge - 1, y);
  generator->MoveMouseTo(right_edge, y);
  UpdateAutoHideStateNow();
  EXPECT_NE(right_edge - 1,
            display::Screen::GetScreen()->GetCursorScreenPoint().x());
  EXPECT_EQ(SHELF_AUTO_HIDE_SHOWN, shelf->GetAutoHideState());

  // Hide the shelf.
  generator->MoveMouseTo(right_edge - 60, y);
  UpdateAutoHideStateNow();
  EXPECT_EQ(SHELF_AUTO_HIDE_HIDDEN, shelf->GetAutoHideState());

  // Moving the mouse to the right edge of the screen crossing the light bar and
  // overshooting by a lot should keep the shelf hidden.
  generator->MoveMouseTo(right_edge - 1, y);
  generator->MoveMouseTo(right_edge + 50, y);
  UpdateAutoHideStateNow();
  EXPECT_EQ(SHELF_AUTO_HIDE_HIDDEN, shelf->GetAutoHideState());

  // Moving the mouse to the right edge of the screen crossing the light bar and
  // overshooting a bit should show the shelf.
  generator->MoveMouseTo(right_edge - 1, y);
  generator->MoveMouseTo(right_edge + 2, y);
  UpdateAutoHideStateNow();
  EXPECT_EQ(SHELF_AUTO_HIDE_SHOWN, shelf->GetAutoHideState());

  // Keeping the mouse close to the left edge of the secondary display after the
  // shelf is shown should keep the shelf shown.
  generator->MoveMouseTo(right_edge + 2, y + 1);
  UpdateAutoHideStateNow();
  EXPECT_EQ(SHELF_AUTO_HIDE_SHOWN, shelf->GetAutoHideState());

  // Moving the mouse far from the left edge of the secondary display should
  // hide the shelf.
  generator->MoveMouseTo(right_edge + 50, y);
  UpdateAutoHideStateNow();
  EXPECT_EQ(SHELF_AUTO_HIDE_HIDDEN, shelf->GetAutoHideState());

  // Moving to the left edge of the secondary display without first crossing
  // the primary display's right aligned shelf first should not show the shelf.
  generator->MoveMouseTo(right_edge + 2, y);
  UpdateAutoHideStateNow();
  EXPECT_EQ(SHELF_AUTO_HIDE_HIDDEN, shelf->GetAutoHideState());
}

// Assertions around the login screen.
TEST_F(ShelfLayoutManagerTest, VisibleWhenLoginScreenShowing) {
  Shelf* shelf = GetPrimaryShelf();
  auto* wallpaper_controller = Shell::Get()->wallpaper_controller();
  WallpaperShownWaiter waiter;

  SessionInfo info;
  info.state = session_manager::SessionState::LOGIN_PRIMARY;
  Shell::Get()->session_controller()->SetSessionInfo(info);
  EXPECT_EQ(SHELF_VISIBLE, shelf->GetVisibilityState());

  // No wallpaper.
  ASSERT_FALSE(wallpaper_controller->HasShownAnyWallpaper());
  EXPECT_EQ(SHELF_BACKGROUND_LOGIN, GetShelfWidget()->GetBackgroundType());

  // Showing wallpaper is asynchronous.
  wallpaper_controller->ShowDefaultWallpaperForTesting();
  waiter.Wait();
  ASSERT_TRUE(wallpaper_controller->HasShownAnyWallpaper());

  // Non-blurred wallpaper.
  wallpaper_controller->UpdateWallpaperBlur(/*blur=*/false);
  EXPECT_EQ(SHELF_BACKGROUND_LOGIN_NONBLURRED_WALLPAPER,
            GetShelfWidget()->GetBackgroundType());

  // Blurred wallpaper.
  wallpaper_controller->UpdateWallpaperBlur(/*blur=*/true);
  EXPECT_EQ(SHELF_BACKGROUND_LOGIN, GetShelfWidget()->GetBackgroundType());
}

// Assertions around the lock screen showing.
TEST_F(ShelfLayoutManagerTest, VisibleWhenLockScreenShowing) {
  Shelf* shelf = GetPrimaryShelf();
  ShelfLayoutManager* layout_manager = GetShelfLayoutManager();
  shelf->SetAutoHideBehavior(SHELF_AUTO_HIDE_BEHAVIOR_ALWAYS);
  views::Widget* widget = CreateTestWidget();
  widget->Maximize();
  EXPECT_EQ(SHELF_AUTO_HIDE, shelf->GetVisibilityState());
  EXPECT_EQ(SHELF_AUTO_HIDE_HIDDEN, shelf->GetAutoHideState());

  // LayoutShelf() forces the animation to completion, at which point the
  // shelf should go off the screen.
  layout_manager->LayoutShelf();
  display::Display display = display::Screen::GetScreen()->GetPrimaryDisplay();
  EXPECT_EQ(display.bounds().bottom() - kHiddenShelfInScreenPortion,
            GetShelfWidget()->GetWindowBoundsInScreen().y());

  std::unique_ptr<views::Widget> lock_widget(AshTestBase::CreateTestWidget(
      nullptr, kShellWindowId_LockScreenContainer, gfx::Rect(200, 200)));
  lock_widget->Maximize();

  // Lock the screen.
  LockScreen();
  // Showing a widget in the lock screen should force the shelf to be visible.
  EXPECT_EQ(SHELF_VISIBLE, shelf->GetVisibilityState());
  EXPECT_EQ(SHELF_BACKGROUND_LOGIN, GetShelfWidget()->GetBackgroundType());

  UnlockScreen();
  EXPECT_EQ(SHELF_AUTO_HIDE, shelf->GetVisibilityState());
  EXPECT_EQ(SHELF_BACKGROUND_DEFAULT, GetShelfWidget()->GetBackgroundType());
}

// Tests that the shelf should be visible when in overview mode.
TEST_F(ShelfLayoutManagerTest, VisibleInOverview) {
  ui::ScopedAnimationDurationScaleMode regular_animations(
      ui::ScopedAnimationDurationScaleMode::NON_ZERO_DURATION);

  std::unique_ptr<aura::Window> window(CreateTestWindow());
  window->Show();
  Shelf* shelf = GetPrimaryShelf();
  shelf->SetAutoHideBehavior(SHELF_AUTO_HIDE_BEHAVIOR_ALWAYS);
  EXPECT_EQ(SHELF_AUTO_HIDE, shelf->GetVisibilityState());
  EXPECT_EQ(SHELF_AUTO_HIDE_HIDDEN, shelf->GetAutoHideState());

  // LayoutShelf() forces the animation to completion, at which point the
  // shelf should go off the screen.
  GetShelfLayoutManager()->LayoutShelf();
  display::Display display = display::Screen::GetScreen()->GetPrimaryDisplay();
  EXPECT_EQ(display.bounds().bottom() - kHiddenShelfInScreenPortion,
            GetShelfWidget()->GetWindowBoundsInScreen().y());

  OverviewController* overview_controller = Shell::Get()->overview_controller();
  // Tests that the shelf is visible when in overview mode
  overview_controller->StartOverview();
  ash::ShellTestApi().WaitForOverviewAnimationState(
      ash::OverviewAnimationState::kEnterAnimationComplete);

  EXPECT_EQ(SHELF_AUTO_HIDE, shelf->GetVisibilityState());
  EXPECT_EQ(SHELF_AUTO_HIDE_SHOWN, shelf->GetAutoHideState());
  EXPECT_EQ(SHELF_BACKGROUND_OVERVIEW, GetShelfWidget()->GetBackgroundType());

  // Test that on exiting overview mode, the shelf returns to auto hide state.
  overview_controller->EndOverview();
  ash::ShellTestApi().WaitForOverviewAnimationState(
      ash::OverviewAnimationState::kExitAnimationComplete);

  EXPECT_EQ(SHELF_AUTO_HIDE, shelf->GetVisibilityState());
  EXPECT_EQ(SHELF_AUTO_HIDE_HIDDEN, shelf->GetAutoHideState());
  EXPECT_EQ(display.bounds().bottom() - kHiddenShelfInScreenPortion,
            GetShelfWidget()->GetNativeWindow()->GetTargetBounds().y());
}

// Assertions around SetAutoHideBehavior.
TEST_F(ShelfLayoutManagerTest, SetAutoHideBehavior) {
  Shelf* shelf = GetPrimaryShelf();
  views::Widget* widget = CreateTestWidget();

  shelf->SetAutoHideBehavior(SHELF_AUTO_HIDE_BEHAVIOR_ALWAYS);
  EXPECT_EQ(SHELF_AUTO_HIDE, shelf->GetVisibilityState());

  shelf->SetAutoHideBehavior(SHELF_AUTO_HIDE_BEHAVIOR_NEVER);
  EXPECT_EQ(SHELF_VISIBLE, shelf->GetVisibilityState());

  widget->Maximize();
  EXPECT_EQ(SHELF_VISIBLE, shelf->GetVisibilityState());
  display::Screen* screen = display::Screen::GetScreen();
  EXPECT_EQ(screen->GetPrimaryDisplay().work_area().bottom(),
            widget->GetWorkAreaBoundsInScreen().bottom());

  shelf->SetAutoHideBehavior(SHELF_AUTO_HIDE_BEHAVIOR_ALWAYS);
  EXPECT_EQ(SHELF_AUTO_HIDE, shelf->GetVisibilityState());
  EXPECT_EQ(screen->GetPrimaryDisplay().work_area().bottom(),
            widget->GetWorkAreaBoundsInScreen().bottom());

  ui::ScopedAnimationDurationScaleMode animation_duration(
      ui::ScopedAnimationDurationScaleMode::SLOW_DURATION);

  shelf->SetAutoHideBehavior(SHELF_AUTO_HIDE_BEHAVIOR_NEVER);
  ShelfWidget* shelf_widget = GetShelfWidget();
  EXPECT_TRUE(shelf_widget->status_area_widget()->IsVisible());
  StepWidgetLayerAnimatorToEnd(shelf_widget);
  StepWidgetLayerAnimatorToEnd(shelf_widget->status_area_widget());
  EXPECT_EQ(SHELF_VISIBLE, shelf->GetVisibilityState());
  EXPECT_EQ(screen->GetPrimaryDisplay().work_area().bottom(),
            widget->GetWorkAreaBoundsInScreen().bottom());
}

// Verifies the shelf is visible when status/shelf is focused.
TEST_F(ShelfLayoutManagerTest, VisibleWhenStatusOrShelfFocused) {
  Shelf* shelf = GetPrimaryShelf();
  views::Widget* widget = CreateTestWidget();
  shelf->SetAutoHideBehavior(SHELF_AUTO_HIDE_BEHAVIOR_ALWAYS);
  EXPECT_EQ(SHELF_AUTO_HIDE, shelf->GetVisibilityState());
  EXPECT_EQ(SHELF_AUTO_HIDE_HIDDEN, shelf->GetAutoHideState());

  // Focus the shelf. Have to go through the focus cycler as normal focus
  // requests to it do nothing.
  GetShelfWidget()->GetFocusCycler()->RotateFocus(FocusCycler::FORWARD);
  EXPECT_EQ(SHELF_AUTO_HIDE_SHOWN, shelf->GetAutoHideState());

  widget->Activate();
  EXPECT_EQ(SHELF_AUTO_HIDE_HIDDEN, shelf->GetAutoHideState());

  // Trying to activate the status should fail, since we only allow activating
  // it when the user is using the keyboard (i.e. through FocusCycler).
  GetShelfWidget()->status_area_widget()->Activate();
  EXPECT_EQ(SHELF_AUTO_HIDE_HIDDEN, shelf->GetAutoHideState());

  GetShelfWidget()->GetFocusCycler()->RotateFocus(FocusCycler::FORWARD);
  EXPECT_EQ(SHELF_AUTO_HIDE_SHOWN, shelf->GetAutoHideState());
}

// Ensure a SHELF_VISIBLE shelf stays visible when the app list is shown.
TEST_F(ShelfLayoutManagerTest, OpenAppListWithShelfVisibleState) {
  Shelf* shelf = GetPrimaryShelf();
  shelf->SetAutoHideBehavior(SHELF_AUTO_HIDE_BEHAVIOR_NEVER);

  // Create a normal unmaximized window; the shelf should be visible.
  aura::Window* window = CreateTestWindow();
  window->SetBounds(gfx::Rect(0, 0, 100, 100));
  window->Show();
  GetAppListTestHelper()->CheckVisibility(false);
  EXPECT_EQ(SHELF_VISIBLE, shelf->GetVisibilityState());

  // Show the app list and the shelf stays visible.
  GetAppListTestHelper()->ShowAndRunLoop(GetPrimaryDisplayId());
  GetAppListTestHelper()->CheckVisibility(true);
  EXPECT_EQ(SHELF_VISIBLE, shelf->GetVisibilityState());

  // Hide the app list and the shelf stays visible.
  GetAppListTestHelper()->DismissAndRunLoop();
  GetAppListTestHelper()->CheckVisibility(false);
  EXPECT_EQ(SHELF_VISIBLE, shelf->GetVisibilityState());
}

// Ensure a SHELF_AUTO_HIDE shelf is shown temporarily (SHELF_AUTO_HIDE_SHOWN)
// when the app list is shown, but the visibility state doesn't change.
TEST_F(ShelfLayoutManagerTest, OpenAppListWithShelfAutoHideState) {
  Shelf* shelf = GetPrimaryShelf();
  shelf->SetAutoHideBehavior(SHELF_AUTO_HIDE_BEHAVIOR_ALWAYS);

  // Create a normal unmaximized window; the shelf should be hidden.
  aura::Window* window = CreateTestWindow();
  window->SetBounds(gfx::Rect(0, 0, 100, 100));
  window->Show();
  GetAppListTestHelper()->CheckVisibility(false);
  EXPECT_EQ(SHELF_AUTO_HIDE, shelf->GetVisibilityState());
  EXPECT_EQ(SHELF_AUTO_HIDE_HIDDEN, shelf->GetAutoHideState());

  // Show the app list and the shelf should be temporarily visible.
  GetAppListTestHelper()->ShowAndRunLoop(GetPrimaryDisplayId());
  // The shelf's auto hide state won't be changed until the timer fires, so
  // force it to update now.
  GetShelfLayoutManager()->UpdateVisibilityState();
  GetAppListTestHelper()->CheckVisibility(true);
  EXPECT_EQ(SHELF_AUTO_HIDE, shelf->GetVisibilityState());
  EXPECT_EQ(SHELF_AUTO_HIDE_SHOWN, shelf->GetAutoHideState());

  // Hide the app list and the shelf should be hidden again.
  GetAppListTestHelper()->DismissAndRunLoop();
  GetAppListTestHelper()->CheckVisibility(false);
  EXPECT_EQ(SHELF_AUTO_HIDE, shelf->GetVisibilityState());
  EXPECT_EQ(SHELF_AUTO_HIDE_HIDDEN, shelf->GetAutoHideState());
}

// Makes sure that when we have dual displays, with one or both shelves are set
// to AutoHide, viewing the AppList on one of them doesn't unhide the other
// hidden shelf.
TEST_F(ShelfLayoutManagerTest, DualDisplayOpenAppListWithShelfAutoHideState) {
  // Create two displays.
  UpdateDisplay("0+0-200x200,+200+0-100x100");
  aura::Window::Windows root_windows = Shell::GetAllRootWindows();
  EXPECT_EQ(root_windows.size(), 2U);

  // Get the shelves in both displays and set them to be 'AutoHide'.
  Shelf* shelf_1 = Shelf::ForWindow(root_windows[0]);
  Shelf* shelf_2 = Shelf::ForWindow(root_windows[1]);
  EXPECT_NE(shelf_1, shelf_2);
  EXPECT_NE(shelf_1->GetWindow()->GetRootWindow(),
            shelf_2->GetWindow()->GetRootWindow());
  shelf_1->SetAutoHideBehavior(SHELF_AUTO_HIDE_BEHAVIOR_ALWAYS);
  shelf_1->shelf_layout_manager()->LayoutShelf();
  shelf_2->SetAutoHideBehavior(SHELF_AUTO_HIDE_BEHAVIOR_ALWAYS);
  shelf_2->shelf_layout_manager()->LayoutShelf();

  // Create a window in each display and show them in maximized state.
  aura::Window* window_1 = CreateTestWindowInParent(root_windows[0]);
  window_1->SetBounds(gfx::Rect(0, 0, 100, 100));
  window_1->SetProperty(aura::client::kShowStateKey, ui::SHOW_STATE_MAXIMIZED);
  window_1->Show();
  aura::Window* window_2 = CreateTestWindowInParent(root_windows[1]);
  window_2->SetBounds(gfx::Rect(201, 0, 100, 100));
  window_2->SetProperty(aura::client::kShowStateKey, ui::SHOW_STATE_MAXIMIZED);
  window_2->Show();

  EXPECT_EQ(shelf_1->GetWindow()->GetRootWindow(), window_1->GetRootWindow());
  EXPECT_EQ(shelf_2->GetWindow()->GetRootWindow(), window_2->GetRootWindow());

  // Activate one window in one display.
  wm::ActivateWindow(window_1);

  Shell::Get()->UpdateShelfVisibility();
  GetAppListTestHelper()->CheckVisibility(false);
  EXPECT_EQ(SHELF_AUTO_HIDE, shelf_1->GetVisibilityState());
  EXPECT_EQ(SHELF_AUTO_HIDE, shelf_2->GetVisibilityState());
  EXPECT_EQ(SHELF_AUTO_HIDE_HIDDEN, shelf_1->GetAutoHideState());
  EXPECT_EQ(SHELF_AUTO_HIDE_HIDDEN, shelf_2->GetAutoHideState());

  // Show the app list; only the shelf on the same display should be shown.
  GetAppListTestHelper()->ShowAndRunLoop(GetPrimaryDisplayId());
  Shell::Get()->UpdateShelfVisibility();
  GetAppListTestHelper()->CheckVisibility(true);
  EXPECT_EQ(SHELF_AUTO_HIDE, shelf_1->GetVisibilityState());
  EXPECT_EQ(SHELF_AUTO_HIDE_SHOWN, shelf_1->GetAutoHideState());
  EXPECT_EQ(SHELF_AUTO_HIDE, shelf_2->GetVisibilityState());
  EXPECT_EQ(SHELF_AUTO_HIDE_HIDDEN, shelf_2->GetAutoHideState());

  // Hide the app list, both shelves should be hidden.
  GetAppListTestHelper()->DismissAndRunLoop();
  Shell::Get()->UpdateShelfVisibility();
  GetAppListTestHelper()->CheckVisibility(false);
  EXPECT_EQ(SHELF_AUTO_HIDE, shelf_1->GetVisibilityState());
  EXPECT_EQ(SHELF_AUTO_HIDE, shelf_2->GetVisibilityState());
  EXPECT_EQ(SHELF_AUTO_HIDE_HIDDEN, shelf_1->GetAutoHideState());
  EXPECT_EQ(SHELF_AUTO_HIDE_HIDDEN, shelf_2->GetAutoHideState());
}

// Ensure a SHELF_HIDDEN shelf (for a fullscreen window) is shown temporarily
// when the app list is shown, and hidden again when the app list is dismissed.
TEST_F(ShelfLayoutManagerTest, OpenAppListWithShelfHiddenState) {
  Shelf* shelf = GetPrimaryShelf();

  // Create a window and make it full screen; the shelf should be hidden.
  aura::Window* window = CreateTestWindow();
  window->SetBounds(gfx::Rect(0, 0, 100, 100));
  window->SetProperty(aura::client::kShowStateKey, ui::SHOW_STATE_FULLSCREEN);
  window->Show();
  wm::ActivateWindow(window);
  GetAppListTestHelper()->CheckVisibility(false);
  EXPECT_EQ(SHELF_HIDDEN, shelf->GetVisibilityState());
  EXPECT_EQ(WorkspaceWindowState::kFullscreen, GetWorkspaceWindowState());
  EXPECT_FALSE(GetNonLockScreenContainersContainerLayer()->GetMasksToBounds());

  // Show the app list and the shelf should be temporarily visible.
  GetAppListTestHelper()->ShowAndRunLoop(GetPrimaryDisplayId());
  GetAppListTestHelper()->CheckVisibility(true);
  EXPECT_EQ(SHELF_AUTO_HIDE, shelf->GetVisibilityState());
  EXPECT_EQ(SHELF_AUTO_HIDE_SHOWN, shelf->GetAutoHideState());

  // Hide the app list and the shelf should be hidden again.
  GetAppListTestHelper()->DismissAndRunLoop();
  GetAppListTestHelper()->CheckVisibility(false);
  EXPECT_EQ(SHELF_HIDDEN, shelf->GetVisibilityState());
}

// Tests the correct behavior of the shelf when there is a system modal window
// open when we have a single display.
TEST_F(ShelfLayoutManagerTest, ShelfWithSystemModalWindowSingleDisplay) {
  Shelf* shelf = GetPrimaryShelf();
  ShelfLayoutManager* layout_manager = GetShelfLayoutManager();
  layout_manager->LayoutShelf();
  shelf->SetAutoHideBehavior(SHELF_AUTO_HIDE_BEHAVIOR_ALWAYS);

  aura::Window* window = CreateTestWindow();
  window->SetBounds(gfx::Rect(0, 0, 100, 100));
  window->SetProperty(aura::client::kShowStateKey, ui::SHOW_STATE_MAXIMIZED);
  window->Show();
  wm::ActivateWindow(window);

  // Enable system modal dialog, and make sure shelf is still hidden.
  ShellTestApi().SimulateModalWindowOpenForTest(true);
  EXPECT_TRUE(Shell::IsSystemModalWindowOpen());
  EXPECT_FALSE(wm::CanActivateWindow(window));
  Shell::Get()->UpdateShelfVisibility();
  EXPECT_EQ(SHELF_AUTO_HIDE, shelf->GetVisibilityState());
  EXPECT_EQ(SHELF_AUTO_HIDE_HIDDEN, shelf->GetAutoHideState());
}

// Tests the correct behavior of the shelf when there is a system modal window
// open when we have dual display.
TEST_F(ShelfLayoutManagerTest, ShelfWithSystemModalWindowDualDisplay) {
  // Create two displays.
  UpdateDisplay("200x200,100x100");
  aura::Window::Windows root_windows = Shell::GetAllRootWindows();
  EXPECT_EQ(2U, root_windows.size());

  // Get the shelves in both displays and set them to be 'AutoHide'.
  Shelf* shelf_1 = Shelf::ForWindow(root_windows[0]);
  Shelf* shelf_2 = Shelf::ForWindow(root_windows[1]);
  EXPECT_NE(shelf_1, shelf_2);
  EXPECT_NE(shelf_1->GetWindow()->GetRootWindow(),
            shelf_2->GetWindow()->GetRootWindow());
  shelf_1->SetAutoHideBehavior(SHELF_AUTO_HIDE_BEHAVIOR_ALWAYS);
  shelf_1->shelf_layout_manager()->LayoutShelf();
  shelf_2->SetAutoHideBehavior(SHELF_AUTO_HIDE_BEHAVIOR_ALWAYS);
  shelf_2->shelf_layout_manager()->LayoutShelf();

  // Create a window in each display and show them in maximized state.
  aura::Window* window_1 = CreateTestWindowInParent(root_windows[0]);
  window_1->SetBounds(gfx::Rect(0, 0, 100, 100));
  window_1->SetProperty(aura::client::kShowStateKey, ui::SHOW_STATE_MAXIMIZED);
  window_1->Show();
  aura::Window* window_2 = CreateTestWindowInParent(root_windows[1]);
  window_2->SetBounds(gfx::Rect(201, 0, 100, 100));
  window_2->SetProperty(aura::client::kShowStateKey, ui::SHOW_STATE_MAXIMIZED);
  window_2->Show();

  EXPECT_EQ(shelf_1->GetWindow()->GetRootWindow(), window_1->GetRootWindow());
  EXPECT_EQ(shelf_2->GetWindow()->GetRootWindow(), window_2->GetRootWindow());
  EXPECT_TRUE(window_1->IsVisible());
  EXPECT_TRUE(window_2->IsVisible());

  // Enable system modal dialog, and make sure both shelves are still hidden.
  ShellTestApi().SimulateModalWindowOpenForTest(true);
  EXPECT_TRUE(Shell::IsSystemModalWindowOpen());
  EXPECT_FALSE(wm::CanActivateWindow(window_1));
  EXPECT_FALSE(wm::CanActivateWindow(window_2));
  Shell::Get()->UpdateShelfVisibility();
  EXPECT_EQ(SHELF_AUTO_HIDE, shelf_1->GetVisibilityState());
  EXPECT_EQ(SHELF_AUTO_HIDE_HIDDEN, shelf_1->GetAutoHideState());
  EXPECT_EQ(SHELF_AUTO_HIDE, shelf_2->GetVisibilityState());
  EXPECT_EQ(SHELF_AUTO_HIDE_HIDDEN, shelf_2->GetAutoHideState());
}

// Tests that the shelf is only hidden for a fullscreen window at the front and
// toggles visibility when another window is activated.
TEST_F(ShelfLayoutManagerTest, FullscreenWindowInFrontHidesShelf) {
  Shelf* shelf = GetPrimaryShelf();
  EXPECT_EQ(WorkspaceWindowState::kDefault, GetWorkspaceWindowState());
  EXPECT_TRUE(GetNonLockScreenContainersContainerLayer()->GetMasksToBounds());

  // Create a window and make it full screen.
  aura::Window* window1 = CreateTestWindow();
  window1->SetBounds(gfx::Rect(0, 0, 100, 100));
  window1->SetProperty(aura::client::kShowStateKey, ui::SHOW_STATE_FULLSCREEN);
  window1->Show();
  EXPECT_EQ(WorkspaceWindowState::kFullscreen, GetWorkspaceWindowState());
  EXPECT_FALSE(GetNonLockScreenContainersContainerLayer()->GetMasksToBounds());

  aura::Window* window2 = CreateTestWindow();
  window2->SetBounds(gfx::Rect(0, 0, 100, 100));
  window2->Show();

  WindowState::Get(window1)->Activate();
  EXPECT_EQ(SHELF_HIDDEN, shelf->GetVisibilityState());

  WindowState::Get(window2)->Activate();
  EXPECT_EQ(SHELF_VISIBLE, shelf->GetVisibilityState());

  WindowState::Get(window1)->Activate();
  EXPECT_EQ(SHELF_HIDDEN, shelf->GetVisibilityState());
}

// Test the behavior of the shelf when a window on one display is fullscreen
// but the other display has the active window.
TEST_F(ShelfLayoutManagerTest, FullscreenWindowOnSecondDisplay) {
  UpdateDisplay("800x600,800x600");
  aura::Window::Windows root_windows = Shell::GetAllRootWindows();

  // Create windows on either display.
  aura::Window* window1 = CreateTestWindow();
  window1->SetBoundsInScreen(gfx::Rect(0, 0, 100, 100),
                             display::Screen::GetScreen()->GetAllDisplays()[0]);
  window1->SetProperty(aura::client::kShowStateKey, ui::SHOW_STATE_FULLSCREEN);
  window1->Show();

  aura::Window* window2 = CreateTestWindow();
  window2->SetBoundsInScreen(gfx::Rect(800, 0, 100, 100),
                             display::Screen::GetScreen()->GetAllDisplays()[1]);
  window2->Show();

  EXPECT_EQ(root_windows[0], window1->GetRootWindow());
  EXPECT_EQ(root_windows[1], window2->GetRootWindow());

  WindowState::Get(window2)->Activate();
  EXPECT_EQ(
      SHELF_HIDDEN,
      Shelf::ForWindow(window1)->shelf_layout_manager()->visibility_state());
  EXPECT_EQ(
      SHELF_VISIBLE,
      Shelf::ForWindow(window2)->shelf_layout_manager()->visibility_state());
}

// Test for Pinned mode.
TEST_F(ShelfLayoutManagerTest, PinnedWindowHidesShelf) {
  Shelf* shelf = GetPrimaryShelf();

  aura::Window* window1 = CreateTestWindow();
  window1->SetBounds(gfx::Rect(0, 0, 100, 100));
  window1->Show();

  EXPECT_EQ(SHELF_VISIBLE, shelf->GetVisibilityState());

  window_util::PinWindow(window1, /* trusted */ false);
  EXPECT_EQ(SHELF_HIDDEN, shelf->GetVisibilityState());

  WindowState::Get(window1)->Restore();
  EXPECT_EQ(SHELF_VISIBLE, shelf->GetVisibilityState());
}

// Tests SHELF_ALIGNMENT_(LEFT, RIGHT).
TEST_F(ShelfLayoutManagerTest, SetAlignment) {
  Shelf* shelf = GetPrimaryShelf();
  ShelfLayoutManager* layout_manager = GetShelfLayoutManager();
  // Force an initial layout.
  shelf->SetAutoHideBehavior(SHELF_AUTO_HIDE_BEHAVIOR_NEVER);
  layout_manager->LayoutShelf();
  EXPECT_EQ(SHELF_VISIBLE, shelf->GetVisibilityState());

  shelf->SetAlignment(SHELF_ALIGNMENT_LEFT);
  gfx::Rect shelf_bounds(GetShelfWidget()->GetWindowBoundsInScreen());
  display::Display display = display::Screen::GetScreen()->GetPrimaryDisplay();
  ASSERT_NE(-1, display.id());
  EXPECT_EQ(layout_manager->GetIdealBounds().width(),
            display.GetWorkAreaInsets().left());
  EXPECT_GE(shelf_bounds.width(),
            GetShelfWidget()->GetContentsView()->GetPreferredSize().width());
  EXPECT_EQ(SHELF_ALIGNMENT_LEFT, GetPrimaryShelf()->alignment());
  EXPECT_EQ(display.work_area(),
            GetPrimaryWorkAreaInsets()->ComputeStableWorkArea());

  StatusAreaWidget* status_area_widget = GetShelfWidget()->status_area_widget();
  gfx::Rect status_bounds(status_area_widget->GetWindowBoundsInScreen());
  // TODO(estade): Re-enable this check. See crbug.com/660928.
  //  EXPECT_GE(
  //      status_bounds.width(),
  //      status_area_widget->GetContentsView()->GetPreferredSize().width());
  EXPECT_EQ(layout_manager->GetIdealBounds().width(),
            display.GetWorkAreaInsets().left());
  EXPECT_EQ(0, display.GetWorkAreaInsets().top());
  EXPECT_EQ(0, display.GetWorkAreaInsets().bottom());
  EXPECT_EQ(0, display.GetWorkAreaInsets().right());
  EXPECT_EQ(display.bounds().x(), shelf_bounds.x());
  EXPECT_EQ(display.bounds().y(), shelf_bounds.y());
  EXPECT_EQ(display.bounds().height(), shelf_bounds.height());
  shelf->SetAutoHideBehavior(SHELF_AUTO_HIDE_BEHAVIOR_ALWAYS);
  display = display::Screen::GetScreen()->GetPrimaryDisplay();
  EXPECT_EQ(0, display.GetWorkAreaInsets().left());
  EXPECT_EQ(0, display.work_area().x());

  shelf->SetAutoHideBehavior(SHELF_AUTO_HIDE_BEHAVIOR_NEVER);
  shelf->SetAlignment(SHELF_ALIGNMENT_RIGHT);
  shelf_bounds = GetShelfWidget()->GetWindowBoundsInScreen();
  display = display::Screen::GetScreen()->GetPrimaryDisplay();
  ASSERT_NE(-1, display.id());
  EXPECT_EQ(layout_manager->GetIdealBounds().width(),
            display.GetWorkAreaInsets().right());
  EXPECT_GE(shelf_bounds.width(),
            GetShelfWidget()->GetContentsView()->GetPreferredSize().width());
  EXPECT_EQ(SHELF_ALIGNMENT_RIGHT, GetPrimaryShelf()->alignment());
  EXPECT_EQ(display.work_area(),
            GetPrimaryWorkAreaInsets()->ComputeStableWorkArea());

  status_bounds = gfx::Rect(status_area_widget->GetWindowBoundsInScreen());
  // TODO(estade): Re-enable this check. See crbug.com/660928.
  //  EXPECT_GE(
  //      status_bounds.width(),
  //      status_area_widget->GetContentsView()->GetPreferredSize().width());
  EXPECT_EQ(layout_manager->GetIdealBounds().width(),
            display.GetWorkAreaInsets().right());
  EXPECT_EQ(0, display.GetWorkAreaInsets().top());
  EXPECT_EQ(0, display.GetWorkAreaInsets().bottom());
  EXPECT_EQ(0, display.GetWorkAreaInsets().left());
  EXPECT_EQ(display.work_area().right(), shelf_bounds.x());
  EXPECT_EQ(display.bounds().y(), shelf_bounds.y());
  EXPECT_EQ(display.bounds().height(), shelf_bounds.height());

  const gfx::Rect stable_work_area =
      display::Screen::GetScreen()->GetPrimaryDisplay().work_area();

  shelf->SetAutoHideBehavior(SHELF_AUTO_HIDE_BEHAVIOR_ALWAYS);
  display = display::Screen::GetScreen()->GetPrimaryDisplay();
  EXPECT_EQ(0, display.GetWorkAreaInsets().right());
  EXPECT_EQ(0, display.bounds().right() - display.work_area().right());
  EXPECT_EQ(stable_work_area,
            GetPrimaryWorkAreaInsets()->ComputeStableWorkArea());
}

TEST_F(ShelfLayoutManagerTest, GestureDrag) {
  // Slop is an implementation detail of gesture recognition, and complicates
  // these tests. Ignore it.
  ui::GestureConfiguration::GetInstance()
      ->set_max_touch_move_in_pixels_for_click(0);
  Shelf* shelf = GetPrimaryShelf();
  {
    SCOPED_TRACE("BOTTOM");
    shelf->SetAlignment(SHELF_ALIGNMENT_BOTTOM);
    gfx::Rect shelf_bounds = GetVisibleShelfWidgetBoundsInScreen();
    gfx::Point bottom_center = shelf_bounds.bottom_center();
    bottom_center.Offset(0, -1);  // Make sure the point is inside shelf.
    RunGestureDragTests(bottom_center, shelf_bounds.top_center());
    GetAppListTestHelper()->WaitUntilIdle();
  }
  {
    SCOPED_TRACE("LEFT");
    shelf->SetAlignment(SHELF_ALIGNMENT_LEFT);
    gfx::Rect shelf_bounds = GetVisibleShelfWidgetBoundsInScreen();
    gfx::Point right_center = shelf_bounds.right_center();
    right_center.Offset(-1, 0);  // Make sure the point is inside shelf.
    RunGestureDragTests(shelf_bounds.left_center(), right_center);
    GetAppListTestHelper()->WaitUntilIdle();
  }
  {
    SCOPED_TRACE("RIGHT");
    shelf->SetAlignment(SHELF_ALIGNMENT_RIGHT);
    gfx::Rect shelf_bounds = GetVisibleShelfWidgetBoundsInScreen();
    gfx::Point right_center = shelf_bounds.right_center();
    right_center.Offset(-1, 0);  // Make sure the point is inside shelf.
    RunGestureDragTests(right_center, shelf_bounds.left_center());
    GetAppListTestHelper()->WaitUntilIdle();
  }
}

TEST_F(ShelfLayoutManagerTest, MouseDrag) {
  Shelf* shelf = GetPrimaryShelf();
  gfx::Rect shelf_bounds_in_screen = GetVisibleShelfWidgetBoundsInScreen();

  // Calculate drag start point and end point. |start_point| and |target_point|
  // make sure that mouse event is received by Shelf/AppListView instead of
  // child views (like HomeButton and SearchBoxView).
  int x = shelf_bounds_in_screen.x() + shelf_bounds_in_screen.width() / 4;
  int y = shelf_bounds_in_screen.CenterPoint().y();
  gfx::Point start_point(x, y);
  gfx::Point target_point = gfx::Point(x, 0);

  auto test_procedure = [this, &start_point, &target_point]() {
    GetAppListTestHelper()->CheckState(ash::AppListViewState::kClosed);

    // Drag AppListView from bottom to top. Check that the final state of
    // AppList is kFullscreenAllApps.
    MouseDragShelfTo(start_point, target_point);
    GetAppListTestHelper()->WaitUntilIdle();
    GetAppListTestHelper()->CheckState(
        ash::AppListViewState::kFullscreenAllApps);

    // Drag AppListView from top to bottom. Check that the AppList is closed
    // after dragging.
    MouseDragShelfTo(target_point, start_point);
    GetAppListTestHelper()->WaitUntilIdle();
    GetAppListTestHelper()->CheckState(ash::AppListViewState::kClosed);
  };

  {
    SCOPED_TRACE("NEVER_AUTO_HIDE");
    GetEventGenerator()->MoveMouseTo(
        GetPrimaryDisplay().bounds().CenterPoint());

    // Check the shelf's default state.
    ASSERT_EQ(SHELF_ALIGNMENT_BOTTOM, shelf->alignment());
    ASSERT_EQ(SHELF_AUTO_HIDE_BEHAVIOR_NEVER, shelf->auto_hide_behavior());
    ASSERT_EQ(SHELF_VISIBLE, shelf->GetVisibilityState());

    // Verifies that dragging AppList view from Shelf works as expected.
    test_procedure();
  }

  {
    SCOPED_TRACE("AUTO_HIDE");
    GetEventGenerator()->MoveMouseTo(
        GetPrimaryDisplay().bounds().CenterPoint());
    shelf->SetAutoHideBehavior(SHELF_AUTO_HIDE_BEHAVIOR_ALWAYS);
    views::Widget* widget = CreateTestWidget();
    widget->Maximize();

    GetShelfLayoutManager()->LayoutShelf();
    ASSERT_EQ(SHELF_AUTO_HIDE_HIDDEN, shelf->GetAutoHideState());
    ASSERT_EQ(SHELF_AUTO_HIDE, shelf->GetVisibilityState());
    MouseMouseToShowAutoHiddenShelf();

    // Verifies that dragging AppList view from Shelf in auto-hide mode works as
    // expected.
    test_procedure();
  }
}

// If swiping up on shelf ends with fling event, the app list state should
// depends on the fling velocity.
TEST_F(ShelfLayoutManagerTest, FlingUpOnShelfForAppList) {
  Shelf* shelf = GetPrimaryShelf();
  EXPECT_EQ(SHELF_ALIGNMENT_BOTTOM, shelf->alignment());
  EXPECT_EQ(SHELF_AUTO_HIDE_BEHAVIOR_NEVER, shelf->auto_hide_behavior());
  EXPECT_EQ(SHELF_VISIBLE, shelf->GetVisibilityState());

  // Starts the drag from the center of the shelf's bottom.
  gfx::Rect shelf_widget_bounds = GetShelfWidget()->GetWindowBoundsInScreen();
  gfx::Point start = shelf_widget_bounds.bottom_center();

  // Fling up that exceeds the velocity threshold should show the fullscreen app
  // list.
  StartScroll(start);
  UpdateScroll(-app_list::AppListView::kDragSnapToPeekingThreshold);
  EndScroll(true /* is_fling */,
            -(app_list::AppListView::kDragVelocityThreshold + 10));
  GetAppListTestHelper()->WaitUntilIdle();
  GetAppListTestHelper()->CheckVisibility(true);
  GetAppListTestHelper()->CheckState(ash::AppListViewState::kFullscreenAllApps);

  // Closing the app list.
  GetAppListTestHelper()->DismissAndRunLoop();
  GetAppListTestHelper()->CheckVisibility(false);
  GetAppListTestHelper()->CheckState(ash::AppListViewState::kClosed);

  // Fling down that exceeds the velocity threshold should close the app list.
  StartScroll(start);
  UpdateScroll(-app_list::AppListView::kDragSnapToPeekingThreshold);
  EndScroll(true /* is_fling */,
            app_list::AppListView::kDragVelocityThreshold + 10);
  GetAppListTestHelper()->WaitUntilIdle();
  GetAppListTestHelper()->CheckVisibility(false);
  GetAppListTestHelper()->CheckState(ash::AppListViewState::kClosed);

  // Fling the app list not exceed the velocity threshold, the state depends on
  // the drag amount.
  StartScroll(start);
  UpdateScroll(-(app_list::AppListView::kDragSnapToPeekingThreshold - 10));
  EndScroll(true /* is_fling */,
            -(app_list::AppListView::kDragVelocityThreshold - 10));
  GetAppListTestHelper()->WaitUntilIdle();
  GetAppListTestHelper()->CheckVisibility(true);
  GetAppListTestHelper()->CheckState(ash::AppListViewState::kPeeking);
}

// Tests that duplicate swipe up from bottom bezel should not make app list
// undraggable. (See https://crbug.com/896934)
TEST_F(ShelfLayoutManagerTest, DuplicateDragUpFromBezel) {
  GetAppListTestHelper()->CheckVisibility(false);
  GetAppListTestHelper()->CheckState(ash::AppListViewState::kClosed);

  // Start the drag from the bottom bezel to the area that snaps to fullscreen
  // state.
  gfx::Rect shelf_widget_bounds = GetShelfWidget()->GetWindowBoundsInScreen();
  gfx::Point start =
      gfx::Point(shelf_widget_bounds.x() + shelf_widget_bounds.width() / 2,
                 shelf_widget_bounds.bottom() + 1);
  gfx::Point end = gfx::Point(
      start.x(), shelf_widget_bounds.bottom() -
                     app_list::AppListView::kDragSnapToPeekingThreshold - 10);
  ui::test::EventGenerator* generator = GetEventGenerator();
  constexpr base::TimeDelta kTimeDelta = base::TimeDelta::FromMilliseconds(100);
  constexpr int kNumScrollSteps = 4;
  generator->GestureScrollSequence(start, end, kTimeDelta, kNumScrollSteps);
  GetAppListTestHelper()->WaitUntilIdle();
  GetAppListTestHelper()->CheckVisibility(true);
  GetAppListTestHelper()->CheckState(ash::AppListViewState::kFullscreenAllApps);

  // Start the same drag event from bezel.
  generator->GestureScrollSequence(start, end, kTimeDelta, kNumScrollSteps);
  GetAppListTestHelper()->WaitUntilIdle();
  GetAppListTestHelper()->CheckVisibility(true);
  GetAppListTestHelper()->CheckState(ash::AppListViewState::kFullscreenAllApps);

  // Start the drag from top screen to the area that snaps to closed state. (The
  // launcher is still draggable now.)
  start.set_y(
      display::Screen::GetScreen()->GetPrimaryDisplay().work_area().y());
  end.set_y(shelf_widget_bounds.bottom() -
            app_list::AppListView::kDragSnapToClosedThreshold + 10);
  generator->GestureScrollSequence(start, end, kTimeDelta, kNumScrollSteps);
  GetAppListTestHelper()->WaitUntilIdle();
  GetAppListTestHelper()->CheckVisibility(false);
  GetAppListTestHelper()->CheckState(ash::AppListViewState::kClosed);
}

// Change the shelf alignment during dragging should dismiss the app list.
TEST_F(ShelfLayoutManagerTest, ChangeShelfAlignmentDuringAppListDragging) {
  Shelf* shelf = GetPrimaryShelf();
  EXPECT_EQ(SHELF_ALIGNMENT_BOTTOM, shelf->alignment());
  EXPECT_EQ(SHELF_AUTO_HIDE_BEHAVIOR_NEVER, shelf->auto_hide_behavior());
  EXPECT_EQ(SHELF_VISIBLE, shelf->GetVisibilityState());

  StartScroll(GetShelfWidget()->GetWindowBoundsInScreen().CenterPoint());
  UpdateScroll(-app_list::AppListView::kDragSnapToPeekingThreshold);
  GetAppListTestHelper()->WaitUntilIdle();
  shelf->SetAlignment(SHELF_ALIGNMENT_LEFT);
  // Note, value -10 here has no specific meaning, it only used to make the
  // event scroll up a little bit.
  UpdateScroll(-10);
  EndScroll(false /* is_fling */, 0.f);
  GetAppListTestHelper()->WaitUntilIdle();
  GetAppListTestHelper()->CheckVisibility(false);
}

TEST_F(ShelfLayoutManagerTest, SwipingUpOnShelfInLaptopModeForAppList) {
  Shelf* shelf = GetPrimaryShelf();
  EXPECT_EQ(SHELF_ALIGNMENT_BOTTOM, shelf->alignment());
  EXPECT_EQ(SHELF_AUTO_HIDE_BEHAVIOR_NEVER, shelf->auto_hide_behavior());
  EXPECT_EQ(SHELF_VISIBLE, shelf->GetVisibilityState());

  ui::test::EventGenerator* generator = GetEventGenerator();
  constexpr base::TimeDelta kTimeDelta = base::TimeDelta::FromMilliseconds(100);
  constexpr int kNumScrollSteps = 4;

  // Starts the drag from the center of the shelf's bottom.
  gfx::Rect shelf_bounds = GetVisibleShelfWidgetBoundsInScreen();
  gfx::Point start = shelf_bounds.bottom_center();
  gfx::Vector2d delta;

  // Swiping up less than the close threshold should close the app list.
  delta.set_y(app_list::AppListView::kDragSnapToClosedThreshold - 10);
  gfx::Point end = start - delta;
  generator->GestureScrollSequence(start, end, kTimeDelta, kNumScrollSteps);
  EXPECT_EQ(SHELF_VISIBLE, shelf->GetVisibilityState());
  GetAppListTestHelper()->WaitUntilIdle();
  GetAppListTestHelper()->CheckVisibility(false);
  GetAppListTestHelper()->CheckState(ash::AppListViewState::kClosed);

  // Swiping up more than the close threshold but less than peeking threshold
  // should keep the app list at PEEKING state.
  delta.set_y(app_list::AppListView::kDragSnapToPeekingThreshold - 10);
  end = start - delta;
  generator->GestureScrollSequence(start, end, kTimeDelta, kNumScrollSteps);
  EXPECT_EQ(SHELF_VISIBLE, shelf->GetVisibilityState());
  GetAppListTestHelper()->WaitUntilIdle();
  GetAppListTestHelper()->CheckVisibility(true);
  GetAppListTestHelper()->CheckState(ash::AppListViewState::kPeeking);

  // Closing the app list.
  GetAppListTestHelper()->DismissAndRunLoop();
  GetAppListTestHelper()->CheckVisibility(false);
  GetAppListTestHelper()->CheckState(ash::AppListViewState::kClosed);

  // Swiping up more than the peeking threshold should keep the app list at
  // FULLSCREEN_ALL_APPS state.
  delta.set_y(app_list::AppListView::kDragSnapToPeekingThreshold + 10);
  end = start - delta;
  generator->GestureScrollSequence(start, end, kTimeDelta, kNumScrollSteps);
  EXPECT_EQ(SHELF_VISIBLE, shelf->GetVisibilityState());
  GetAppListTestHelper()->WaitUntilIdle();
  GetAppListTestHelper()->CheckVisibility(true);
  GetAppListTestHelper()->CheckState(ash::AppListViewState::kFullscreenAllApps);

  // Closing the app list.
  GetAppListTestHelper()->DismissAndRunLoop();
  GetAppListTestHelper()->CheckVisibility(false);
  GetAppListTestHelper()->CheckState(ash::AppListViewState::kClosed);

  shelf->SetAutoHideBehavior(SHELF_AUTO_HIDE_BEHAVIOR_ALWAYS);
  // Create a normal unmaximized window, the auto-hide shelf should be hidden.
  aura::Window* window = CreateTestWindow();
  window->SetBounds(gfx::Rect(0, 0, 100, 100));
  window->Show();
  GetAppListTestHelper()->CheckVisibility(false);
  EXPECT_EQ(SHELF_AUTO_HIDE, shelf->GetVisibilityState());
  EXPECT_EQ(SHELF_AUTO_HIDE_HIDDEN, shelf->GetAutoHideState());

  // Swiping up to show the auto-hide shelf.
  end = shelf_bounds.top_center();
  generator->GestureScrollSequence(start, end, kTimeDelta, kNumScrollSteps);
  EXPECT_EQ(SHELF_AUTO_HIDE_SHOWN, shelf->GetAutoHideState());

  // Swiping up on the auto-hide shelf to drag up the app list. Close the app
  // list on drag ended since the short drag distance but keep the previous
  // shown auto-hide shelf still visible.
  delta.set_y(app_list::AppListView::kDragSnapToClosedThreshold - 10);
  end = start - delta;
  generator->GestureScrollSequence(start, end, kTimeDelta, kNumScrollSteps);
  GetAppListTestHelper()->WaitUntilIdle();
  GetAppListTestHelper()->CheckVisibility(false);
  EXPECT_EQ(SHELF_AUTO_HIDE_SHOWN, shelf->GetAutoHideState());
}

// Swiping on shelf when fullscreen app list is opened should have no effect.
TEST_F(ShelfLayoutManagerTest, SwipingOnShelfIfAppListOpened) {
  Shelf* shelf = GetPrimaryShelf();
  ShelfLayoutManager* layout_manager = GetShelfLayoutManager();
  layout_manager->OnAppListVisibilityChanged(true, GetPrimaryDisplayId());
  EXPECT_EQ(SHELF_ALIGNMENT_BOTTOM, shelf->alignment());
  EXPECT_EQ(SHELF_AUTO_HIDE_BEHAVIOR_NEVER, shelf->auto_hide_behavior());
  EXPECT_EQ(SHELF_VISIBLE, shelf->GetVisibilityState());

  // Note: A window must be visible in order to hide the shelf.
  CreateTestWidget();

  ui::test::EventGenerator* generator = GetEventGenerator();
  constexpr base::TimeDelta kTimeDelta = base::TimeDelta::FromMilliseconds(100);
  constexpr int kNumScrollSteps = 4;
  gfx::Point start = GetShelfWidget()->GetWindowBoundsInScreen().CenterPoint();

  // Swiping down on shelf when the fullscreen app list is opened
  // should not hide the shelf.
  gfx::Point end = start + gfx::Vector2d(0, 120);
  generator->GestureScrollSequence(start, end, kTimeDelta, kNumScrollSteps);
  EXPECT_EQ(SHELF_VISIBLE, shelf->GetVisibilityState());
  EXPECT_EQ(SHELF_AUTO_HIDE_BEHAVIOR_NEVER, shelf->auto_hide_behavior());

  // Swiping left on shelf when the fullscreen app list is opened
  // should not hide the shelf.
  shelf->SetAlignment(SHELF_ALIGNMENT_LEFT);
  end = start + gfx::Vector2d(-120, 0);
  generator->GestureScrollSequence(start, end, kTimeDelta, kNumScrollSteps);
  EXPECT_EQ(SHELF_VISIBLE, shelf->GetVisibilityState());
  EXPECT_EQ(SHELF_AUTO_HIDE_BEHAVIOR_NEVER, shelf->auto_hide_behavior());

  // Swiping right on shelf when the fullscreen app list is opened
  // should not hide the shelf.
  shelf->SetAlignment(SHELF_ALIGNMENT_RIGHT);
  end = start + gfx::Vector2d(120, 0);
  generator->GestureScrollSequence(start, end, kTimeDelta, kNumScrollSteps);
  EXPECT_EQ(SHELF_VISIBLE, shelf->GetVisibilityState());
  EXPECT_EQ(SHELF_AUTO_HIDE_BEHAVIOR_NEVER, shelf->auto_hide_behavior());
}

TEST_F(ShelfLayoutManagerTest, WindowVisibilityDisablesAutoHide) {
  UpdateDisplay("800x600,800x600");
  Shelf* shelf = GetPrimaryShelf();
  ShelfLayoutManager* layout_manager = GetShelfLayoutManager();
  layout_manager->LayoutShelf();
  shelf->SetAutoHideBehavior(SHELF_AUTO_HIDE_BEHAVIOR_ALWAYS);

  // Create a visible window so auto-hide behavior is enforced
  views::Widget* dummy = CreateTestWidget();

  // Window visible => auto hide behaves normally.
  layout_manager->UpdateVisibilityState();
  EXPECT_EQ(SHELF_AUTO_HIDE_HIDDEN, shelf->GetAutoHideState());

  // Window minimized => auto hide disabled.
  dummy->Minimize();
  EXPECT_EQ(SHELF_AUTO_HIDE_SHOWN, shelf->GetAutoHideState());

  // Window closed => auto hide disabled.
  dummy->CloseNow();
  EXPECT_EQ(SHELF_AUTO_HIDE_SHOWN, shelf->GetAutoHideState());

  // Multiple window test
  views::Widget* window1 = CreateTestWidget();
  views::Widget* window2 = CreateTestWidget();

  // both visible => normal autohide
  EXPECT_EQ(SHELF_AUTO_HIDE_HIDDEN, shelf->GetAutoHideState());

  // either minimzed => normal autohide
  window2->Minimize();
  EXPECT_EQ(SHELF_AUTO_HIDE_HIDDEN, shelf->GetAutoHideState());
  window2->Restore();
  window1->Minimize();
  EXPECT_EQ(SHELF_AUTO_HIDE_HIDDEN, shelf->GetAutoHideState());

  // both minimized => disable auto hide
  window2->Minimize();
  EXPECT_EQ(SHELF_AUTO_HIDE_SHOWN, shelf->GetAutoHideState());

  // Test moving windows to/from other display.
  window2->Restore();
  EXPECT_EQ(SHELF_AUTO_HIDE_HIDDEN, shelf->GetAutoHideState());
  // Move to second display.
  window2->SetBounds(gfx::Rect(850, 50, 50, 50));
  EXPECT_EQ(SHELF_AUTO_HIDE_SHOWN, shelf->GetAutoHideState());
  // Move back to primary display.
  window2->SetBounds(gfx::Rect(50, 50, 50, 50));
  EXPECT_EQ(SHELF_AUTO_HIDE_HIDDEN, shelf->GetAutoHideState());
}

// Tests the shelf animates back to its original visible bounds when it is
// dragged down but there are no visible windows.
TEST_F(ShelfLayoutManagerTest,
       ShelfAnimatesWhenGestureCompleteNoVisibleWindow) {
  Shelf* shelf = GetPrimaryShelf();
  shelf->SetAutoHideBehavior(SHELF_AUTO_HIDE_BEHAVIOR_ALWAYS);
  EXPECT_EQ(SHELF_AUTO_HIDE, shelf->GetVisibilityState());
  EXPECT_EQ(SHELF_AUTO_HIDE_SHOWN, shelf->GetAutoHideState());
  gfx::Rect visible_bounds = GetShelfWidget()->GetWindowBoundsInScreen();
  {
    // Enable animations so that we can make sure that they occur.
    ui::ScopedAnimationDurationScaleMode regular_animations(
        ui::ScopedAnimationDurationScaleMode::NON_ZERO_DURATION);

    ui::test::EventGenerator* generator = GetEventGenerator();
    gfx::Rect shelf_bounds_in_screen =
        GetShelfWidget()->GetWindowBoundsInScreen();
    gfx::Point start(shelf_bounds_in_screen.CenterPoint());
    gfx::Point end(start.x(), shelf_bounds_in_screen.bottom());
    generator->GestureScrollSequence(start, end,
                                     base::TimeDelta::FromMilliseconds(10), 5);
    EXPECT_EQ(SHELF_AUTO_HIDE, shelf->GetVisibilityState());
    EXPECT_EQ(SHELF_AUTO_HIDE_SHOWN, shelf->GetAutoHideState());

    ShelfAnimationWaiter waiter(visible_bounds);
    // Wait till the animation completes and check that it occurred.
    waiter.WaitTillDoneAnimating();
    EXPECT_TRUE(waiter.WasValidAnimation());
  }
}

// Tests that the shelf animates to the visible bounds after a swipe up on
// the auto hidden shelf.
TEST_F(ShelfLayoutManagerTest, ShelfAnimatesToVisibleWhenGestureInComplete) {
  Shelf* shelf = GetPrimaryShelf();
  shelf->SetAutoHideBehavior(SHELF_AUTO_HIDE_BEHAVIOR_ALWAYS);
  EXPECT_EQ(SHELF_AUTO_HIDE, shelf->GetVisibilityState());
  EXPECT_EQ(SHELF_AUTO_HIDE_SHOWN, shelf->GetAutoHideState());
  gfx::Rect visible_bounds = GetShelfWidget()->GetWindowBoundsInScreen();

  // Create a visible window, otherwise the shelf will not hide.
  CreateTestWidget();

  // Get the bounds of the shelf when it is hidden.
  EXPECT_EQ(SHELF_AUTO_HIDE, shelf->GetVisibilityState());
  EXPECT_EQ(SHELF_AUTO_HIDE_HIDDEN, shelf->GetAutoHideState());

  {
    // Enable the animations so that we can make sure they do occur.
    ui::ScopedAnimationDurationScaleMode regular_animations(
        ui::ScopedAnimationDurationScaleMode::NON_ZERO_DURATION);

    display::Display display =
        display::Screen::GetScreen()->GetPrimaryDisplay();
    gfx::Point start = display.bounds().bottom_center();
    gfx::Point end(start.x(), start.y() - 100);
    ui::test::EventGenerator* generator = GetEventGenerator();

    generator->GestureScrollSequence(start, end,
                                     base::TimeDelta::FromMilliseconds(10), 1);
    EXPECT_EQ(SHELF_AUTO_HIDE, shelf->GetVisibilityState());
    EXPECT_EQ(SHELF_AUTO_HIDE_SHOWN, shelf->GetAutoHideState());
    ShelfAnimationWaiter waiter(visible_bounds);
    waiter.WaitTillDoneAnimating();
    EXPECT_TRUE(waiter.WasValidAnimation());
  }
}

// Tests that the shelf animates to the auto hidden bounds after a swipe down
// on the visible shelf.
TEST_F(ShelfLayoutManagerTest, ShelfAnimatesToHiddenWhenGestureOutComplete) {
  Shelf* shelf = GetPrimaryShelf();
  shelf->SetAutoHideBehavior(SHELF_AUTO_HIDE_BEHAVIOR_ALWAYS);
  EXPECT_EQ(SHELF_AUTO_HIDE, shelf->GetVisibilityState());
  EXPECT_EQ(SHELF_AUTO_HIDE_SHOWN, shelf->GetAutoHideState());
  gfx::Rect visible_bounds = GetShelfWidget()->GetWindowBoundsInScreen();

  // Create a visible window, otherwise the shelf will not hide.
  CreateTestWidget();

  gfx::Rect auto_hidden_bounds = GetShelfWidget()->GetWindowBoundsInScreen();

  {
    // Enable the animations so that we can make sure they do occur.
    ui::ScopedAnimationDurationScaleMode regular_animations(
        ui::ScopedAnimationDurationScaleMode::NON_ZERO_DURATION);

    ui::test::EventGenerator* generator = GetEventGenerator();

    // Show the shelf first.
    display::Display display =
        display::Screen::GetScreen()->GetPrimaryDisplay();
    ShelfAnimationWaiter waiter1(visible_bounds);
    generator->MoveMouseTo(display.bounds().bottom_center());
    waiter1.WaitTillDoneAnimating();
    EXPECT_EQ(SHELF_AUTO_HIDE_SHOWN, shelf->GetAutoHideState());

    // Now swipe down.
    gfx::Point start =
        GetShelfWidget()->GetWindowBoundsInScreen().CenterPoint();
    gfx::Point end = gfx::Point(start.x(), start.y() + 100);
    generator->GestureScrollSequence(start, end,
                                     base::TimeDelta::FromMilliseconds(10), 1);
    EXPECT_EQ(SHELF_AUTO_HIDE, shelf->GetVisibilityState());
    EXPECT_EQ(SHELF_AUTO_HIDE_HIDDEN, shelf->GetAutoHideState());
    ShelfAnimationWaiter waiter2(auto_hidden_bounds);
    waiter2.WaitTillDoneAnimating();
    EXPECT_TRUE(waiter2.WasValidAnimation());
  }
}

TEST_F(ShelfLayoutManagerTest, AutohideShelfForAutohideWhenActiveWindow) {
  Shelf* shelf = GetPrimaryShelf();

  views::Widget* widget_one = CreateTestWidget();
  views::Widget* widget_two = CreateTestWidget();
  aura::Window* window_two = widget_two->GetNativeWindow();

  // Turn on hide_shelf_when_active behavior for window two - shelf should
  // still be visible when window two is made active since it is not yet
  // maximized.
  widget_one->Activate();
  EXPECT_EQ(SHELF_VISIBLE, shelf->GetVisibilityState());
  WindowState::Get(window_two)
      ->set_autohide_shelf_when_maximized_or_fullscreen(true);
  widget_two->Activate();
  EXPECT_EQ(SHELF_VISIBLE, shelf->GetVisibilityState());

  // Now the flag takes effect once window two is maximized.
  widget_two->Maximize();
  EXPECT_EQ(SHELF_AUTO_HIDE, shelf->GetVisibilityState());

  // The hide_shelf_when_active flag should override the behavior of the
  // hide_shelf_when_fullscreen flag even if the window is currently fullscreen.
  WindowState::Get(window_two)->SetHideShelfWhenFullscreen(false);
  widget_two->SetFullscreen(true);
  EXPECT_EQ(SHELF_AUTO_HIDE, shelf->GetVisibilityState());
  WindowState::Get(window_two)->Restore();

  // With the flag off, shelf no longer auto-hides.
  widget_one->Activate();
  EXPECT_EQ(SHELF_VISIBLE, shelf->GetVisibilityState());
  WindowState::Get(window_two)
      ->set_autohide_shelf_when_maximized_or_fullscreen(false);
  widget_two->Activate();
  EXPECT_EQ(SHELF_VISIBLE, shelf->GetVisibilityState());

  WindowState::Get(window_two)
      ->set_autohide_shelf_when_maximized_or_fullscreen(true);
  window_two->SetProperty(aura::client::kZOrderingKey,
                          ui::ZOrderLevel::kFloatingWindow);

  auto* shelf_window = shelf->GetWindow();
  aura::Window* container = shelf_window->GetRootWindow()->GetChildById(
      kShellWindowId_AlwaysOnTopContainer);
  EXPECT_TRUE(base::Contains(container->children(), window_two));

  widget_two->Maximize();
  EXPECT_EQ(SHELF_AUTO_HIDE, shelf->GetVisibilityState());

  EXPECT_EQ(WorkspaceWindowState::kMaximized, GetWorkspaceWindowState());
  EXPECT_FALSE(GetNonLockScreenContainersContainerLayer()->GetMasksToBounds());
}

TEST_F(ShelfLayoutManagerTest, ShelfFlickerOnTrayActivation) {
  Shelf* shelf = GetPrimaryShelf();

  // Create a visible window so auto-hide behavior is enforced.
  CreateTestWidget();

  // Turn on auto-hide for the shelf.
  shelf->SetAutoHideBehavior(SHELF_AUTO_HIDE_BEHAVIOR_ALWAYS);
  EXPECT_EQ(SHELF_AUTO_HIDE, shelf->GetVisibilityState());
  EXPECT_EQ(SHELF_AUTO_HIDE_HIDDEN, shelf->GetAutoHideState());

  // Show the status menu. That should make the shelf visible again.
  Shell::Get()->accelerator_controller()->PerformActionIfEnabled(
      TOGGLE_SYSTEM_TRAY_BUBBLE, {});
  EXPECT_EQ(SHELF_AUTO_HIDE, shelf->GetVisibilityState());
  EXPECT_EQ(SHELF_AUTO_HIDE_SHOWN, shelf->GetAutoHideState());
  EXPECT_TRUE(GetPrimaryUnifiedSystemTray()->IsBubbleShown());
}

TEST_F(ShelfLayoutManagerTest, WorkAreaChangeWorkspace) {
  // Make sure the shelf is always visible.
  Shelf* shelf = GetPrimaryShelf();
  ShelfLayoutManager* layout_manager = GetShelfLayoutManager();
  shelf->SetAutoHideBehavior(SHELF_AUTO_HIDE_BEHAVIOR_NEVER);
  layout_manager->LayoutShelf();

  views::Widget* widget_one = CreateTestWidget();
  widget_one->Maximize();

  views::Widget* widget_two = CreateTestWidget();
  widget_two->Maximize();
  widget_two->Activate();

  // Both windows are maximized. They should be of the same size.
  EXPECT_EQ(widget_one->GetNativeWindow()->bounds().ToString(),
            widget_two->GetNativeWindow()->bounds().ToString());
  int area_when_shelf_shown =
      widget_one->GetNativeWindow()->bounds().size().GetArea();

  // Now hide the shelf.
  shelf->SetAutoHideBehavior(SHELF_AUTO_HIDE_BEHAVIOR_ALWAYS);

  // Both windows should be resized according to the shelf status.
  EXPECT_EQ(widget_one->GetNativeWindow()->bounds().ToString(),
            widget_two->GetNativeWindow()->bounds().ToString());
  // Resized to small.
  EXPECT_LT(area_when_shelf_shown,
            widget_one->GetNativeWindow()->bounds().size().GetArea());

  // Now show the shelf.
  shelf->SetAutoHideBehavior(SHELF_AUTO_HIDE_BEHAVIOR_NEVER);

  // Again both windows should be of the same size.
  EXPECT_EQ(widget_one->GetNativeWindow()->bounds().ToString(),
            widget_two->GetNativeWindow()->bounds().ToString());
  EXPECT_EQ(area_when_shelf_shown,
            widget_one->GetNativeWindow()->bounds().size().GetArea());
}

TEST_F(ShelfLayoutManagerTest, BackgroundTypeWhenLockingScreen) {
  // Creates a maximized window to have a background type other than default.
  std::unique_ptr<aura::Window> window(CreateTestWindow());
  window->Show();
  wm::ActivateWindow(window.get());
  EXPECT_EQ(SHELF_BACKGROUND_DEFAULT, GetShelfWidget()->GetBackgroundType());
  window->SetProperty(aura::client::kShowStateKey, ui::SHOW_STATE_MAXIMIZED);
  EXPECT_EQ(SHELF_BACKGROUND_MAXIMIZED, GetShelfWidget()->GetBackgroundType());

  Shell::Get()->lock_state_controller()->StartLockAnimationAndLockImmediately();
  EXPECT_EQ(SHELF_BACKGROUND_DEFAULT, GetShelfWidget()->GetBackgroundType());
}

TEST_F(ShelfLayoutManagerTest, WorkspaceMask) {
  std::unique_ptr<aura::Window> w1(CreateTestWindow());
  w1->Show();
  EXPECT_EQ(WorkspaceWindowState::kDefault, GetWorkspaceWindowState());
  EXPECT_TRUE(GetNonLockScreenContainersContainerLayer()->GetMasksToBounds());

  // Overlaps with shelf should not cause any specific behavior.
  w1->SetBounds(GetShelfLayoutManager()->GetIdealBounds());
  EXPECT_EQ(WorkspaceWindowState::kDefault, GetWorkspaceWindowState());
  EXPECT_TRUE(GetNonLockScreenContainersContainerLayer()->GetMasksToBounds());

  w1->SetProperty(aura::client::kShowStateKey, ui::SHOW_STATE_MAXIMIZED);
  EXPECT_EQ(WorkspaceWindowState::kMaximized, GetWorkspaceWindowState());
  EXPECT_FALSE(GetNonLockScreenContainersContainerLayer()->GetMasksToBounds());

  w1->SetProperty(aura::client::kShowStateKey, ui::SHOW_STATE_FULLSCREEN);
  EXPECT_EQ(WorkspaceWindowState::kFullscreen, GetWorkspaceWindowState());
  EXPECT_FALSE(GetNonLockScreenContainersContainerLayer()->GetMasksToBounds());

  std::unique_ptr<aura::Window> w2(CreateTestWindow());
  w2->Show();
  EXPECT_EQ(WorkspaceWindowState::kDefault, GetWorkspaceWindowState());
  EXPECT_TRUE(GetNonLockScreenContainersContainerLayer()->GetMasksToBounds());

  w2.reset();
  EXPECT_EQ(WorkspaceWindowState::kFullscreen, GetWorkspaceWindowState());
  EXPECT_FALSE(GetNonLockScreenContainersContainerLayer()->GetMasksToBounds());

  w1->SetProperty(aura::client::kShowStateKey, ui::SHOW_STATE_NORMAL);
  EXPECT_EQ(WorkspaceWindowState::kDefault, GetWorkspaceWindowState());
  EXPECT_TRUE(GetNonLockScreenContainersContainerLayer()->GetMasksToBounds());
}

TEST_F(ShelfLayoutManagerTest, ShelfBackgroundColor) {
  EXPECT_EQ(SHELF_BACKGROUND_DEFAULT, GetShelfWidget()->GetBackgroundType());

  std::unique_ptr<aura::Window> w1(CreateTestWindow());
  w1->Show();
  wm::ActivateWindow(w1.get());
  EXPECT_EQ(SHELF_BACKGROUND_DEFAULT, GetShelfWidget()->GetBackgroundType());
  w1->SetProperty(aura::client::kShowStateKey, ui::SHOW_STATE_MAXIMIZED);
  EXPECT_EQ(SHELF_BACKGROUND_MAXIMIZED, GetShelfWidget()->GetBackgroundType());

  std::unique_ptr<aura::Window> w2(CreateTestWindow());
  w2->Show();
  wm::ActivateWindow(w2.get());
  // Overlaps with shelf.
  w2->SetBounds(GetShelfLayoutManager()->GetIdealBounds());

  // Still background is 'maximized'.
  EXPECT_EQ(SHELF_BACKGROUND_MAXIMIZED, GetShelfWidget()->GetBackgroundType());

  w1->SetProperty(aura::client::kShowStateKey, ui::SHOW_STATE_MINIMIZED);
  EXPECT_EQ(SHELF_BACKGROUND_DEFAULT, GetShelfWidget()->GetBackgroundType());
  w2->SetProperty(aura::client::kShowStateKey, ui::SHOW_STATE_MINIMIZED);
  EXPECT_EQ(SHELF_BACKGROUND_DEFAULT, GetShelfWidget()->GetBackgroundType());

  w1->SetProperty(aura::client::kShowStateKey, ui::SHOW_STATE_MAXIMIZED);
  EXPECT_EQ(SHELF_BACKGROUND_MAXIMIZED, GetShelfWidget()->GetBackgroundType());

  std::unique_ptr<aura::Window> w3(CreateTestWindow());
  w3->SetProperty(aura::client::kModalKey, ui::MODAL_TYPE_WINDOW);
  ::wm::AddTransientChild(w1.get(), w3.get());
  w3->Show();
  wm::ActivateWindow(w3.get());

  EXPECT_EQ(WorkspaceWindowState::kMaximized, GetWorkspaceWindowState());
  EXPECT_FALSE(GetNonLockScreenContainersContainerLayer()->GetMasksToBounds());

  w3.reset();
  w1.reset();
  EXPECT_EQ(SHELF_BACKGROUND_DEFAULT, GetShelfWidget()->GetBackgroundType());
}

// Tests that the shelf background gets updated when the AppList stays open
// during the tablet mode transition with a visible window.
TEST_F(ShelfLayoutManagerTest, TabletModeTransitionWithAppListVisible) {
  // Home Launcher requires an internal display.
  display::test::DisplayManagerTestApi(Shell::Get()->display_manager())
      .SetFirstDisplayAsInternalDisplay();

  // Show a window, which will later fill the whole screen.
  std::unique_ptr<aura::Window> window(
      AshTestBase::CreateTestWindow(gfx::Rect(0, 0, 400, 400)));
  window->SetProperty(aura::client::kResizeBehaviorKey,
                      aura::client::kResizeBehaviorCanResize |
                          aura::client::kResizeBehaviorCanMaximize);
  wm::ActivateWindow(window.get());

  // Show the AppList over |window|.
  GetAppListTestHelper()->ShowAndRunLoop(GetPrimaryDisplayId());

  // Transition to tablet mode.
  TabletModeControllerTestApi().EnterTabletMode();

  // |window| should be maximized, and the shelf background should match the
  // maximized state.
  EXPECT_EQ(WorkspaceWindowState::kMaximized, GetWorkspaceWindowState());
  EXPECT_EQ(SHELF_BACKGROUND_MAXIMIZED, GetShelfWidget()->GetBackgroundType());
}

// Verify that the shelf doesn't have the opaque background if it's auto-hide
// status.
TEST_F(ShelfLayoutManagerTest, ShelfBackgroundColorAutoHide) {
  EXPECT_EQ(SHELF_BACKGROUND_DEFAULT, GetShelfWidget()->GetBackgroundType());

  GetPrimaryShelf()->SetAutoHideBehavior(SHELF_AUTO_HIDE_BEHAVIOR_ALWAYS);
  std::unique_ptr<aura::Window> w1(CreateTestWindow());
  w1->Show();
  wm::ActivateWindow(w1.get());
  EXPECT_EQ(SHELF_BACKGROUND_DEFAULT, GetShelfWidget()->GetBackgroundType());
  w1->SetProperty(aura::client::kShowStateKey, ui::SHOW_STATE_MAXIMIZED);
  EXPECT_EQ(SHELF_BACKGROUND_DEFAULT, GetShelfWidget()->GetBackgroundType());
}

// Verify the hit bounds of the status area extend to the edge of the shelf.
TEST_F(ShelfLayoutManagerTest, StatusAreaHitBoxCoversEdge) {
  StatusAreaWidget* status_area_widget = GetShelfWidget()->status_area_widget();
  ui::test::EventGenerator* generator = GetEventGenerator();
  display::Display display = display::Screen::GetScreen()->GetPrimaryDisplay();
  gfx::Rect inset_display_bounds = display.bounds();
  inset_display_bounds.Inset(0, 0, 1, 1);

  // Test bottom right pixel for bottom alignment.
  GetPrimaryShelf()->SetAlignment(SHELF_ALIGNMENT_BOTTOM);
  generator->MoveMouseTo(inset_display_bounds.bottom_right());
  EXPECT_FALSE(status_area_widget->IsMessageBubbleShown());
  generator->ClickLeftButton();
  EXPECT_TRUE(status_area_widget->IsMessageBubbleShown());
  generator->ClickLeftButton();
  EXPECT_FALSE(status_area_widget->IsMessageBubbleShown());

  // Test bottom right pixel for right alignment.
  GetPrimaryShelf()->SetAlignment(SHELF_ALIGNMENT_RIGHT);
  generator->MoveMouseTo(inset_display_bounds.bottom_right());
  EXPECT_FALSE(status_area_widget->IsMessageBubbleShown());
  generator->ClickLeftButton();
  EXPECT_TRUE(status_area_widget->IsMessageBubbleShown());
  generator->ClickLeftButton();
  EXPECT_FALSE(status_area_widget->IsMessageBubbleShown());

  // Test bottom left pixel for left alignment.
  generator->MoveMouseTo(inset_display_bounds.bottom_left());
  GetPrimaryShelf()->SetAlignment(SHELF_ALIGNMENT_LEFT);
  EXPECT_FALSE(status_area_widget->IsMessageBubbleShown());
  generator->ClickLeftButton();
  EXPECT_TRUE(status_area_widget->IsMessageBubbleShown());
  generator->ClickLeftButton();
  EXPECT_FALSE(status_area_widget->IsMessageBubbleShown());
}

// Tests that when the auto-hide behaviour is changed during an animation the
// target bounds are updated to reflect the new state.
TEST_F(ShelfLayoutManagerTest,
       ShelfAutoHideToggleDuringAnimationUpdatesBounds) {
  aura::Window* status_window =
      GetShelfWidget()->status_area_widget()->GetNativeView();
  gfx::Rect initial_bounds = status_window->bounds();

  ui::ScopedAnimationDurationScaleMode regular_animations(
      ui::ScopedAnimationDurationScaleMode::SLOW_DURATION);
  GetPrimaryShelf()->SetAutoHideBehavior(SHELF_AUTO_HIDE_ALWAYS_HIDDEN);
  gfx::Rect hide_target_bounds = status_window->GetTargetBounds();
  EXPECT_GT(hide_target_bounds.y(), initial_bounds.y());

  GetPrimaryShelf()->SetAutoHideBehavior(SHELF_AUTO_HIDE_BEHAVIOR_NEVER);
  gfx::Rect reshow_target_bounds = status_window->GetTargetBounds();
  EXPECT_EQ(initial_bounds, reshow_target_bounds);
}

// Tests that during shutdown, that window activation changes are properly
// handled, and do not crash (crbug.com/458768)
TEST_F(ShelfLayoutManagerTest, ShutdownHandlesWindowActivation) {
  GetPrimaryShelf()->SetAutoHideBehavior(SHELF_AUTO_HIDE_BEHAVIOR_ALWAYS);

  aura::Window* window1 = CreateTestWindowInShellWithId(0);
  window1->SetBounds(gfx::Rect(0, 0, 100, 100));
  window1->SetProperty(aura::client::kShowStateKey, ui::SHOW_STATE_MAXIMIZED);
  window1->Show();
  std::unique_ptr<aura::Window> window2(CreateTestWindowInShellWithId(0));
  window2->SetBounds(gfx::Rect(0, 0, 100, 100));
  window2->Show();
  wm::ActivateWindow(window1);

  GetShelfLayoutManager()->PrepareForShutdown();

  // Deleting a focused maximized window will switch focus to |window2|. This
  // would normally cause the ShelfLayoutManager to update its state. However
  // during shutdown we want to handle this without crashing.
  delete window1;
}

TEST_F(ShelfLayoutManagerTest, ShelfLayoutInUnifiedDesktop) {
  Shell::Get()->display_manager()->SetUnifiedDesktopEnabled(true);
  UpdateDisplay("500x400, 500x400");

  StatusAreaWidget* status_area_widget = GetShelfWidget()->status_area_widget();
  EXPECT_TRUE(status_area_widget->IsVisible());
  // Shelf should be in the first display's area.
  gfx::Rect status_area_bounds(status_area_widget->GetWindowBoundsInScreen());
  EXPECT_TRUE(gfx::Rect(0, 0, 500, 400).Contains(status_area_bounds));
  EXPECT_EQ(gfx::Point(500, 400), status_area_bounds.bottom_right());
}

// Tests that the always shown shelf forwards the appropriate events to the home
// launcher gesture handler to handle.
TEST_F(ShelfLayoutManagerTest, HomeLauncherGestureHandler) {
  TestHomeLauncherGestureHandler(/*autohide_shelf=*/false);
}

// Tests that the auto-hide shelf keeps visible and forwards the appropriate
// events to the home launcher gesture handler to handle.
TEST_F(ShelfLayoutManagerTest, HomeLauncherGestureHandlerAutoHideShelf) {
  TestHomeLauncherGestureHandler(/*autohide_shelf=*/true);
}

// Tests that the auto-hide shelf has expected behavior when pressing the
// AppList button while the shelf is being dragged by gesture (see
// https://crbug.com/953877).
TEST_F(ShelfLayoutManagerTest, PressAppListBtnWhenAutoHideShelfBeingDragged) {
  // Create a widget to hide the shelf in auto-hide mode.
  CreateTestWidget();
  GetPrimaryShelf()->SetAutoHideBehavior(SHELF_AUTO_HIDE_BEHAVIOR_ALWAYS);
  EXPECT_FALSE(GetPrimaryShelf()->IsVisible());

  // Emulate to drag the shelf to show it.
  gfx::Point gesture_location = display::Screen::GetScreen()
                                    ->GetPrimaryDisplay()
                                    .bounds()
                                    .bottom_center();
  int delta_y = -1;
  base::TimeTicks timestamp = base::TimeTicks::Now();

  ui::GestureEvent start_event = ui::GestureEvent(
      gesture_location.x(), gesture_location.y(), ui::EF_NONE, timestamp,
      ui::GestureEventDetails(ui::ET_GESTURE_SCROLL_BEGIN, 0, delta_y));
  GetShelfLayoutManager()->ProcessGestureEventOfAutoHideShelf(
      &start_event, GetShelfWidget()->GetNativeView());
  gesture_location.Offset(0, delta_y);

  // Ensure that Shelf is higher than the default height, required by the bug
  // reproduction procedures.
  delta_y = -ShelfConstants::shelf_size() - 1;

  timestamp += base::TimeDelta::FromMilliseconds(200);
  ui::GestureEvent update_event = ui::GestureEvent(
      gesture_location.x(), gesture_location.y(), ui::EF_NONE, timestamp,
      ui::GestureEventDetails(ui::ET_GESTURE_SCROLL_UPDATE, 0, delta_y));
  GetShelfLayoutManager()->ProcessGestureEventOfAutoHideShelf(
      &update_event, GetShelfWidget()->GetNativeView());

  // Emulate to press the AppList button while dragging the Shelf.
  PressHomeButton();
  EXPECT_TRUE(GetPrimaryShelf()->IsVisible());

  // Release the press.
  delta_y -= 1;
  gesture_location.Offset(0, delta_y);
  timestamp += base::TimeDelta::FromMilliseconds(200);
  ui::GestureEvent scroll_end_event = ui::GestureEvent(
      gesture_location.x(), gesture_location.y(), ui::EF_NONE, timestamp,
      ui::GestureEventDetails(ui::ET_GESTURE_SCROLL_END));
  GetShelfLayoutManager()->ProcessGestureEventOfAutoHideShelf(
      &scroll_end_event, GetShelfWidget()->GetNativeView());
  ui::GestureEvent gesture_end_event =
      ui::GestureEvent(gesture_location.x(), gesture_location.y(), ui::EF_NONE,
                       timestamp, ui::GestureEventDetails(ui::ET_GESTURE_END));
  GetShelfLayoutManager()->ProcessGestureEventOfAutoHideShelf(
      &gesture_end_event, GetShelfWidget()->GetNativeView());

  // Press the AppList button to hide the AppList and Shelf. Check the following
  // things:
  // (1) Shelf is hidden
  // (2) Shelf has correct bounds in screen coordinate.
  PressHomeButton();
  EXPECT_EQ(GetScreenAvailableBounds().bottom_left() +
                gfx::Point(0, -kHiddenShelfInScreenPortion).OffsetFromOrigin(),
            GetPrimaryShelf()
                ->GetShelfViewForTesting()
                ->GetBoundsInScreen()
                .origin());
  EXPECT_FALSE(GetPrimaryShelf()->IsVisible());
}

// Tests that the shelf has expected bounds when dragging the shelf by gesture
// and pressing the AppList button by mouse during drag (see
// https://crbug.com/968768).
TEST_F(ShelfLayoutManagerTest, MousePressAppListBtnWhenShelfBeingDragged) {
  // Drag the shelf upward. Notice that in order to drag shelf instead of
  // AppList from shelf, we need to drag the shelf downward a little bit then
  // upward. Because the bug is related with RootView, the event should be sent
  // through the shelf widget.
  gfx::Point gesture_location =
      GetPrimaryShelf()->GetShelfViewForTesting()->bounds().CenterPoint();
  int delta_y = 1;
  base::TimeTicks timestamp = base::TimeTicks::Now();
  ui::GestureEvent start_event = ui::GestureEvent(
      gesture_location.x(), gesture_location.y(), ui::EF_NONE, timestamp,
      ui::GestureEventDetails(ui::ET_GESTURE_SCROLL_BEGIN, 0, delta_y));
  GetPrimaryShelf()->shelf_widget()->OnGestureEvent(&start_event);
  delta_y = -5;
  timestamp += base::TimeDelta::FromMilliseconds(200);
  ui::GestureEvent update_event = ui::GestureEvent(
      gesture_location.x(), gesture_location.y(), ui::EF_NONE, timestamp,
      ui::GestureEventDetails(ui::ET_GESTURE_SCROLL_UPDATE, 0, delta_y));
  GetPrimaryShelf()->shelf_widget()->OnGestureEvent(&update_event);

  // Press the AppList button by mouse.
  views::View* home_button =
      GetPrimaryShelf()->GetShelfViewForTesting()->GetHomeButton();
  GetEventGenerator()->MoveMouseTo(
      home_button->GetBoundsInScreen().CenterPoint());
  GetEventGenerator()->ClickLeftButton();

  // End the gesture event.
  delta_y -= 1;
  gesture_location.Offset(0, delta_y);
  timestamp += base::TimeDelta::FromMilliseconds(200);
  ui::GestureEvent scroll_end_event = ui::GestureEvent(
      gesture_location.x(), gesture_location.y(), ui::EF_NONE, timestamp,
      ui::GestureEventDetails(ui::ET_GESTURE_SCROLL_END));
  GetPrimaryShelf()->shelf_widget()->OnGestureEvent(&scroll_end_event);

  // Verify that the shelf has expected bounds.
  EXPECT_EQ(GetScreenAvailableBounds().bottom_left() +
                gfx::Point(0, -ShelfConstants::shelf_size()).OffsetFromOrigin(),
            GetPrimaryShelf()
                ->GetShelfViewForTesting()
                ->GetBoundsInScreen()
                .origin());
}

// Tests that tap outside of the AUTO_HIDE_SHOWN shelf should hide it.
TEST_F(ShelfLayoutManagerTest, TapOutsideOfAutoHideShownShelf) {
  views::Widget* widget = CreateTestWidget();
  Shelf* shelf = GetPrimaryShelf();
  ui::test::EventGenerator* generator = GetEventGenerator();

  shelf->SetAutoHideBehavior(SHELF_AUTO_HIDE_BEHAVIOR_ALWAYS);
  ShelfLayoutManager* layout_manager = GetShelfLayoutManager();
  layout_manager->LayoutShelf();
  EXPECT_EQ(SHELF_AUTO_HIDE, shelf->GetVisibilityState());
  EXPECT_EQ(SHELF_AUTO_HIDE_HIDDEN, shelf->GetAutoHideState());

  aura::Window* window = widget->GetNativeWindow();
  gfx::Rect window_bounds = window->GetBoundsInScreen();
  gfx::Rect display_bounds =
      display::Screen::GetScreen()->GetPrimaryDisplay().bounds();
  const gfx::Point start(display_bounds.bottom_center());
  const gfx::Point end(start + gfx::Vector2d(0, -80));
  const base::TimeDelta kTimeDelta = base::TimeDelta::FromMilliseconds(100);
  const int kNumScrollSteps = 4;
  // Swipe up to show the auto-hide shelf.
  generator->GestureScrollSequence(start, end, kTimeDelta, kNumScrollSteps);
  EXPECT_EQ(SHELF_AUTO_HIDE_SHOWN, shelf->GetAutoHideState());

  // Tap outside the window and AUTO_HIDE_SHOWN shelf should hide the shelf.
  gfx::Point tap_location =
      window_bounds.bottom_right() + gfx::Vector2d(10, 10);
  generator->GestureTapAt(tap_location);
  EXPECT_EQ(SHELF_AUTO_HIDE_HIDDEN, shelf->GetAutoHideState());

  // Swipe up to show the auto-hide shelf again.
  generator->GestureScrollSequence(start, end, kTimeDelta, kNumScrollSteps);
  EXPECT_EQ(SHELF_AUTO_HIDE_SHOWN, shelf->GetAutoHideState());

  // Tap inside the AUTO_HIDE_SHOWN shelf should not hide the shelf.
  gfx::Rect shelf_bounds = GetShelfWidget()->GetWindowBoundsInScreen();
  tap_location = gfx::Point(shelf_bounds.CenterPoint());
  generator->GestureTapAt(tap_location);
  EXPECT_EQ(SHELF_AUTO_HIDE_SHOWN, shelf->GetAutoHideState());

  // Tap inside the window should still hide the shelf.
  tap_location = window_bounds.origin() + gfx::Vector2d(10, 10);
  generator->GestureTapAt(tap_location);
  EXPECT_EQ(SHELF_AUTO_HIDE_HIDDEN, shelf->GetAutoHideState());

  // Swipe up to show the auto-hide shelf again.
  generator->GestureScrollSequence(start, end, kTimeDelta, kNumScrollSteps);
  EXPECT_EQ(SHELF_AUTO_HIDE_SHOWN, shelf->GetAutoHideState());

  // Tap the system tray that inside the status area should not hide the shelf
  // but open the systrem tray bubble.
  generator->GestureTapAt(
      GetPrimaryUnifiedSystemTray()->GetBoundsInScreen().CenterPoint());
  EXPECT_EQ(SHELF_AUTO_HIDE_SHOWN, shelf->GetAutoHideState());
  EXPECT_TRUE(GetPrimaryUnifiedSystemTray()->IsBubbleShown());
}

// Tests that swiping up on the AUTO_HIDE_HIDDEN shelf, with various speeds,
// offsets, and angles, always shows the shelf.
TEST_F(ShelfLayoutManagerTest, SwipeUpAutoHideHiddenShelf) {
  ui::test::EventGenerator* generator = GetEventGenerator();
  Shelf* shelf = GetPrimaryShelf();

  // Create a window so that the shelf will hide.
  const aura::Window* window = CreateTestWidget()->GetNativeWindow();
  const gfx::Point tap_to_hide_shelf_location =
      window->GetBoundsInScreen().CenterPoint();
  const gfx::Rect display_bounds =
      display::Screen::GetScreen()->GetPrimaryDisplay().bounds();

  shelf->SetAutoHideBehavior(SHELF_AUTO_HIDE_BEHAVIOR_ALWAYS);
  ShelfLayoutManager* layout_manager = GetShelfLayoutManager();
  layout_manager->LayoutShelf();
  EXPECT_EQ(SHELF_AUTO_HIDE, shelf->GetVisibilityState());
  EXPECT_EQ(SHELF_AUTO_HIDE_HIDDEN, shelf->GetAutoHideState());

  const int time_deltas[] = {10, 50, 100, 500};
  const int num_scroll_steps[] = {2, 5, 10, 50};
  const int x_offsets[] = {10, 20, 50};
  const int y_offsets[] = {70, 100, 300, 500};

  for (int time_delta : time_deltas) {
    for (int num_scroll_steps : num_scroll_steps) {
      for (int x_offset : x_offsets) {
        for (int y_offset : y_offsets) {
          const gfx::Point start(display_bounds.bottom_center());
          const gfx::Point end(start + gfx::Vector2d(x_offset, -y_offset));
          generator->GestureScrollSequence(
              start, end, base::TimeDelta::FromMilliseconds(time_delta),
              num_scroll_steps);
          EXPECT_EQ(SHELF_AUTO_HIDE_SHOWN, shelf->GetAutoHideState())
              << "Failure to show shelf after a swipe up in " << time_delta
              << "ms, " << num_scroll_steps << " steps, " << x_offset
              << " X-offset and " << y_offset << " Y-offset.";
          generator->GestureTapAt(tap_to_hide_shelf_location);
          EXPECT_EQ(SHELF_AUTO_HIDE_HIDDEN, shelf->GetAutoHideState());
        }
      }
    }
  }
}

// Tests the auto-hide shelf status when moving the mouse in and out.
TEST_F(ShelfLayoutManagerTest, AutoHideShelfOnMouseMove) {
  // Create one window, or the shelf won't auto-hide.
  CreateTestWidget();
  Shelf* shelf = GetPrimaryShelf();
  ui::test::EventGenerator* generator = GetEventGenerator();
  display::Display display = display::Screen::GetScreen()->GetPrimaryDisplay();

  // Set the shelf to auto-hide.
  shelf->SetAutoHideBehavior(SHELF_AUTO_HIDE_BEHAVIOR_ALWAYS);
  ShelfLayoutManager* layout_manager = GetShelfLayoutManager();
  layout_manager->LayoutShelf();
  EXPECT_EQ(SHELF_AUTO_HIDE, shelf->GetVisibilityState());
  EXPECT_EQ(SHELF_AUTO_HIDE_HIDDEN, shelf->GetAutoHideState());

  // Swipe up to show the shelf.
  SwipeUpToShowShelf();
  EXPECT_EQ(SHELF_AUTO_HIDE_SHOWN, shelf->GetAutoHideState());

  // Move the mouse far away from the shelf, but without having been on the
  // shelf first. This isn't technically a mouse-out event, so the shelf should
  // not hide.
  generator->MoveMouseTo(0, 0);
  ASSERT_FALSE(TriggerAutoHideTimeout());
  EXPECT_EQ(SHELF_AUTO_HIDE_SHOWN, shelf->GetAutoHideState());

  // Now place the mouse on the shelf, then move away. The shelf should hide.
  generator->MoveMouseTo(1, display.bounds().bottom() - 1);
  generator->MoveMouseTo(0, 0);
  ASSERT_TRUE(TriggerAutoHideTimeout());
  EXPECT_EQ(SHELF_AUTO_HIDE_HIDDEN, shelf->GetAutoHideState());

  // Now let's show the shelf again.
  SwipeUpToShowShelf();
  EXPECT_EQ(SHELF_AUTO_HIDE_SHOWN, shelf->GetAutoHideState());

  // Move the mouse away, but move it back within the shelf immediately. The
  // shelf should remain shown.
  generator->MoveMouseTo(0, 0);
  EXPECT_EQ(SHELF_AUTO_HIDE_SHOWN, shelf->GetAutoHideState());
  generator->MoveMouseTo(1, display.bounds().bottom() - 1);
  ASSERT_FALSE(TriggerAutoHideTimeout());
  EXPECT_EQ(SHELF_AUTO_HIDE_SHOWN, shelf->GetAutoHideState());
}

// Verifies that after showing the system tray by shortcut, the shelf item still
// responds to the gesture event. (see https://crbug.com/921182)
TEST_F(ShelfLayoutManagerTest, ShelfItemRespondToGestureEvent) {
  // Prepare for the auto-hide shelf test.
  views::Widget* widget = CreateTestWidget();
  widget->Maximize();
  Shelf* shelf = GetPrimaryShelf();
  ui::test::EventGenerator* generator = GetEventGenerator();
  generator->MoveMouseTo(0, 0);

  // Add ShelfItem.
  ShelfController* controller = Shell::Get()->shelf_controller();
  const std::string app_id("app_id");
  ShelfItem item;
  item.type = TYPE_APP;
  item.id = ShelfID(app_id);
  const int index = controller->model()->Add(item);

  // Turn on the auto-hide mode for shelf. Check the initial states.
  shelf->SetAutoHideBehavior(SHELF_AUTO_HIDE_BEHAVIOR_ALWAYS);
  ShelfLayoutManager* layout_manager = GetShelfLayoutManager();
  layout_manager->LayoutShelf();
  EXPECT_EQ(SHELF_AUTO_HIDE, shelf->GetVisibilityState());
  EXPECT_EQ(SHELF_AUTO_HIDE_HIDDEN, shelf->GetAutoHideState());

  // Show the system tray with the shortcut. Expect that the shelf is shown
  // after triggering the accelerator.
  generator->PressKey(ui::VKEY_S, ui::EF_SHIFT_DOWN | ui::EF_ALT_DOWN);
  generator->ReleaseKey(ui::VKEY_S, ui::EF_SHIFT_DOWN | ui::EF_ALT_DOWN);
  EXPECT_EQ(SHELF_AUTO_HIDE_SHOWN, shelf->GetAutoHideState());

  // Tap on the shelf button. Expect that the shelf button responds to gesture
  // events.
  base::UserActionTester user_action_tester;
  user_action_tester.ResetCounts();
  auto shelf_test_api = std::make_unique<ShelfViewTestAPI>(
      GetPrimaryShelf()->GetShelfViewForTesting());
  views::View* shelf_view = shelf_test_api->GetViewAt(index);
  gfx::Point shelf_btn_center = shelf_view->GetBoundsInScreen().CenterPoint();
  generator->GestureTapAt(shelf_btn_center);
  EXPECT_EQ(1, user_action_tester.GetActionCount("Launcher_ClickOnApp"));
}

// Tests the auto-hide shelf status with mouse events.
TEST_F(ShelfLayoutManagerTest, AutoHideShelfOnMouseEvents) {
  views::Widget* widget = CreateTestWidget();
  widget->Maximize();
  Shelf* shelf = GetPrimaryShelf();
  ui::test::EventGenerator* generator = GetEventGenerator();
  generator->MoveMouseTo(0, 0);

  shelf->SetAutoHideBehavior(SHELF_AUTO_HIDE_BEHAVIOR_ALWAYS);
  ShelfLayoutManager* layout_manager = GetShelfLayoutManager();
  layout_manager->LayoutShelf();
  EXPECT_EQ(SHELF_AUTO_HIDE, shelf->GetVisibilityState());
  EXPECT_EQ(SHELF_AUTO_HIDE_HIDDEN, shelf->GetAutoHideState());

  // Swipe up to show the auto-hide shelf.
  SwipeUpToShowShelf();
  EXPECT_EQ(SHELF_AUTO_HIDE_SHOWN, shelf->GetAutoHideState());

  // Move the mouse should not hide the AUTO_HIDE_SHOWN shelf immediately.
  generator->MoveMouseTo(5, 5);
  EXPECT_EQ(SHELF_AUTO_HIDE_SHOWN, shelf->GetAutoHideState());

  // Mouse press outside the shelf should hide the AUTO_HIDE_SHOWN shelf.
  generator->PressLeftButton();
  generator->ReleaseLeftButton();
  EXPECT_EQ(SHELF_AUTO_HIDE_HIDDEN, shelf->GetAutoHideState());

  // Move the mouse to the position which is contained by the bounds of the
  // shelf when it is visible should show the auto-hide shelf.
  display::Display display = display::Screen::GetScreen()->GetPrimaryDisplay();
  const int display_bottom = display.bounds().bottom();
  generator->MoveMouseTo(1, display_bottom - 1);
  ASSERT_TRUE(TriggerAutoHideTimeout());
  EXPECT_EQ(SHELF_AUTO_HIDE_SHOWN, shelf->GetAutoHideState());

  // Mouse press inside the shelf should not hide the AUTO_HIDE_SHOWN shelf.
  generator->MoveMouseTo(
      GetShelfWidget()->GetWindowBoundsInScreen().CenterPoint());
  generator->PressLeftButton();
  generator->ReleaseLeftButton();
  EXPECT_EQ(SHELF_AUTO_HIDE_SHOWN, shelf->GetAutoHideState());

  // Mouse press the system tray should open the system tray bubble.
  generator->MoveMouseTo(
      GetPrimaryUnifiedSystemTray()->GetBoundsInScreen().CenterPoint());
  generator->PressLeftButton();
  generator->ReleaseLeftButton();
  EXPECT_TRUE(GetPrimaryUnifiedSystemTray()->IsBubbleShown());
}

// Tests that tap shelf item in auto-hide shelf should do nothing.
TEST_F(ShelfLayoutManagerTest, TapShelfItemInAutoHideShelf) {
  Shelf* shelf = GetPrimaryShelf();
  shelf->SetAutoHideBehavior(SHELF_AUTO_HIDE_BEHAVIOR_ALWAYS);

  // Create a normal unmaximized window; the shelf should be hidden.
  aura::Window* window = CreateTestWindow();
  window->SetBounds(gfx::Rect(0, 0, 100, 100));
  window->Show();
  GetAppListTestHelper()->CheckVisibility(false);
  EXPECT_EQ(SHELF_AUTO_HIDE, shelf->GetVisibilityState());
  EXPECT_EQ(SHELF_AUTO_HIDE_HIDDEN, shelf->GetAutoHideState());

  // Tap home button should not open the app list and shelf should keep
  // hidden.
  gfx::Rect home_button_bounds =
      shelf->GetShelfViewForTesting()->GetHomeButton()->GetBoundsInScreen();
  gfx::Rect display_bounds =
      display::Screen::GetScreen()->GetPrimaryDisplay().bounds();
  home_button_bounds.Intersect(display_bounds);
  GetEventGenerator()->GestureTapAt(home_button_bounds.CenterPoint());
  GetAppListTestHelper()->CheckVisibility(false);
  EXPECT_EQ(SHELF_AUTO_HIDE_HIDDEN, shelf->GetAutoHideState());
}

// Tests the a11y feedback for entering/exiting fullscreen workspace state.
TEST_F(ShelfLayoutManagerTest, A11yAlertOnWorkspaceState) {
  TestAccessibilityControllerClient client;
  std::unique_ptr<aura::Window> window1(
      AshTestBase::CreateToplevelTestWindow());
  std::unique_ptr<aura::Window> window2(
      AshTestBase::CreateToplevelTestWindow());
  EXPECT_NE(AccessibilityAlert::WORKSPACE_FULLSCREEN_STATE_ENTERED,
            client.last_a11y_alert());

  // Toggle the current normal window in workspace to fullscreen should send the
  // ENTERED alert.
  const WMEvent fullscreen(WM_EVENT_TOGGLE_FULLSCREEN);
  WindowState* window_state2 = WindowState::Get(window2.get());
  window_state2->OnWMEvent(&fullscreen);
  EXPECT_TRUE(window_state2->IsFullscreen());
  EXPECT_EQ(AccessibilityAlert::WORKSPACE_FULLSCREEN_STATE_ENTERED,
            client.last_a11y_alert());

  // Toggle the current fullscreen'ed window in workspace to exit fullscreen
  // should send the EXITED alert.
  window_state2->OnWMEvent(&fullscreen);
  EXPECT_FALSE(window_state2->IsFullscreen());
  EXPECT_EQ(AccessibilityAlert::WORKSPACE_FULLSCREEN_STATE_EXITED,
            client.last_a11y_alert());

  // Fullscreen the |window2| again to prepare for the following tests.
  window_state2->OnWMEvent(&fullscreen);
  EXPECT_TRUE(window_state2->IsFullscreen());
  EXPECT_EQ(AccessibilityAlert::WORKSPACE_FULLSCREEN_STATE_ENTERED,
            client.last_a11y_alert());
  // Changes the current window in workspace from a fullscreen window to a
  // normal window should send the EXITD alert.
  window_state2->Minimize();
  EXPECT_EQ(AccessibilityAlert::WORKSPACE_FULLSCREEN_STATE_EXITED,
            client.last_a11y_alert());

  // Changes the current window in workspace from a normal window to fullscreen
  // window should send ENTERED alert.
  window_state2->Unminimize();
  EXPECT_TRUE(window_state2->IsFullscreen());
  EXPECT_EQ(AccessibilityAlert::WORKSPACE_FULLSCREEN_STATE_ENTERED,
            client.last_a11y_alert());
}

// Verifies the auto-hide shelf is hidden if there is only a single PIP window.
TEST_F(ShelfLayoutManagerTest, AutoHideShelfHiddenForSinglePipWindow) {
  Shelf* shelf = GetPrimaryShelf();
  shelf->SetAutoHideBehavior(SHELF_AUTO_HIDE_BEHAVIOR_ALWAYS);
  EXPECT_EQ(SHELF_AUTO_HIDE, shelf->GetVisibilityState());
  EXPECT_EQ(SHELF_AUTO_HIDE_SHOWN, shelf->GetAutoHideState());

  // Create a PIP window.
  aura::Window* window = CreateTestWindow();
  window->SetBounds(gfx::Rect(0, 0, 100, 100));
  // Set always on top so it is put in the PIP container.
  window->SetProperty(aura::client::kZOrderingKey,
                      ui::ZOrderLevel::kFloatingWindow);
  window->Show();
  const WMEvent pip_event(WM_EVENT_PIP);
  WindowState::Get(window)->OnWMEvent(&pip_event);
  Shell::Get()->UpdateShelfVisibility();

  // Expect the shelf to be hidden.
  EXPECT_EQ(SHELF_AUTO_HIDE, shelf->GetVisibilityState());
  EXPECT_EQ(SHELF_AUTO_HIDE_HIDDEN, shelf->GetAutoHideState());
}

class ShelfLayoutManagerKeyboardTest : public AshTestBase {
 public:
  ShelfLayoutManagerKeyboardTest() = default;
  ~ShelfLayoutManagerKeyboardTest() override = default;

  // AshTestBase:
  void SetUp() override {
    AshTestBase::SetUp();
    UpdateDisplay("800x600");
    keyboard::SetTouchKeyboardEnabled(true);
    keyboard::SetAccessibilityKeyboardEnabled(true);
  }

  // AshTestBase:
  void TearDown() override {
    keyboard::SetAccessibilityKeyboardEnabled(false);
    keyboard::SetTouchKeyboardEnabled(false);
    AshTestBase::TearDown();
  }

  void InitKeyboardBounds() {
    gfx::Rect work_area(
        display::Screen::GetScreen()->GetPrimaryDisplay().work_area());
    keyboard_bounds_.SetRect(work_area.x(),
                             work_area.y() + work_area.height() / 2,
                             work_area.width(), work_area.height() / 2);
  }

  void NotifyKeyboardChanging(ShelfLayoutManager* layout_manager,
                              bool is_locked,
                              const gfx::Rect& bounds_in_screen) {
    WorkAreaInsets* work_area_insets = GetPrimaryWorkAreaInsets();
    KeyboardStateDescriptor state;
    state.visual_bounds = bounds_in_screen;
    state.occluded_bounds_in_screen = bounds_in_screen;
    state.displaced_bounds_in_screen =
        is_locked ? bounds_in_screen : gfx::Rect();
    state.is_visible = !bounds_in_screen.IsEmpty();
    work_area_insets->OnKeyboardVisibilityChanged(state.is_visible);
    work_area_insets->OnKeyboardAppearanceChanged(state);
  }

  const gfx::Rect& keyboard_bounds() const { return keyboard_bounds_; }

 private:
  gfx::Rect keyboard_bounds_;

  DISALLOW_COPY_AND_ASSIGN(ShelfLayoutManagerKeyboardTest);
};

TEST_F(ShelfLayoutManagerKeyboardTest, ShelfNotMoveOnKeyboardOpen) {
  gfx::Rect orig_bounds = GetShelfWidget()->GetWindowBoundsInScreen();

  ShelfLayoutManager* layout_manager = GetShelfLayoutManager();
  InitKeyboardBounds();
  auto* kb_controller = keyboard::KeyboardUIController::Get();
  // Open keyboard in non-sticky mode.
  kb_controller->ShowKeyboard(false);
  NotifyKeyboardChanging(layout_manager, false, keyboard_bounds());

  // Shelf position should not be changed.
  EXPECT_EQ(orig_bounds, GetShelfWidget()->GetWindowBoundsInScreen());
}

// When kAshUseNewVKWindowBehavior flag enabled, do not change accessibility
// keyboard work area in non-sticky mode.
TEST_F(ShelfLayoutManagerKeyboardTest,
       ShelfIgnoreWorkAreaChangeInNonStickyMode) {
  ShelfLayoutManager* layout_manager = GetShelfLayoutManager();
  InitKeyboardBounds();
  auto* kb_controller = keyboard::KeyboardUIController::Get();
  gfx::Rect orig_work_area(
      display::Screen::GetScreen()->GetPrimaryDisplay().work_area());

  // Open keyboard in non-sticky mode.
  kb_controller->ShowKeyboard(false);
  NotifyKeyboardChanging(layout_manager, false, keyboard_bounds());

  // Work area should not be changed.
  EXPECT_EQ(orig_work_area,
            display::Screen::GetScreen()->GetPrimaryDisplay().work_area());

  kb_controller->HideKeyboardExplicitlyBySystem();
  NotifyKeyboardChanging(layout_manager, false, gfx::Rect());
  EXPECT_EQ(orig_work_area,
            display::Screen::GetScreen()->GetPrimaryDisplay().work_area());
}

// Change accessibility keyboard work area in sticky mode.
TEST_F(ShelfLayoutManagerKeyboardTest, ShelfShouldChangeWorkAreaInStickyMode) {
  ShelfLayoutManager* layout_manager = GetShelfLayoutManager();
  InitKeyboardBounds();
  auto* kb_controller = keyboard::KeyboardUIController::Get();
  gfx::Rect orig_work_area(
      display::Screen::GetScreen()->GetPrimaryDisplay().work_area());

  // Open keyboard in sticky mode.
  kb_controller->ShowKeyboard(true);
  NotifyKeyboardChanging(layout_manager, true, keyboard_bounds());

  // Work area should be changed.
  EXPECT_NE(orig_work_area,
            display::Screen::GetScreen()->GetPrimaryDisplay().work_area());

  // Hide the keyboard.
  kb_controller->HideKeyboardByUser();
  NotifyKeyboardChanging(layout_manager, true, gfx::Rect());

  // Work area should be reset to its original value.
  EXPECT_EQ(orig_work_area,
            display::Screen::GetScreen()->GetPrimaryDisplay().work_area());
}

namespace {

class OverviewAnimationWaiter : public OverviewObserver {
 public:
  OverviewAnimationWaiter() {
    Shell::Get()->overview_controller()->AddObserver(this);
  }

  ~OverviewAnimationWaiter() override {
    Shell::Get()->overview_controller()->RemoveObserver(this);
  }

  // Note this could only be called once because RunLoop would not run after
  // Quit is called. Create a new instance if there's need to wait again.
  void Wait() { run_loop_.Run(); }

  // OverviewObserver:
  void OnOverviewModeStartingAnimationComplete(bool cancel) override {
    run_loop_.Quit();
  }
  void OnOverviewModeEndingAnimationComplete(bool cancel) override {
    run_loop_.Quit();
  }

 private:
  base::RunLoop run_loop_;

  DISALLOW_COPY_AND_ASSIGN(OverviewAnimationWaiter);
};

}  // namespace

// Make sure we don't update the work area during overview animation
// (crbug.com/947343).
TEST_F(ShelfLayoutManagerTest, NoShelfUpdateDuringOverviewAnimation) {
  // Finish lid detection task.
  base::RunLoop().RunUntilIdle();
  TabletModeControllerTestApi().EnterTabletMode();
  // Run overview animations.
  ui::ScopedAnimationDurationScaleMode regular_animations(
      ui::ScopedAnimationDurationScaleMode::NON_ZERO_DURATION);

  std::unique_ptr<aura::Window> window1(AshTestBase::CreateTestWindow());
  std::unique_ptr<aura::Window> fullscreen(AshTestBase::CreateTestWindow());
  fullscreen->SetProperty(aura::client::kShowStateKey,
                          ui::SHOW_STATE_FULLSCREEN);
  wm::ActivateWindow(fullscreen.get());

  OverviewController* overview_controller = Shell::Get()->overview_controller();
  TestDisplayObserver observer;
  {
    OverviewAnimationWaiter waiter;
    overview_controller->StartOverview();
    waiter.Wait();
  }
  ASSERT_TRUE(TabletModeControllerTestApi().IsTabletModeStarted());
  EXPECT_EQ(0, observer.metrics_change_count());
  {
    OverviewAnimationWaiter waiter;
    overview_controller->EndOverview();
    waiter.Wait();
  }
  ASSERT_TRUE(TabletModeControllerTestApi().IsTabletModeStarted());
  EXPECT_EQ(0, observer.metrics_change_count());
}

// Tests that shelf bounds is updated properly after overview animation.
TEST_F(ShelfLayoutManagerTest, ShelfBoundsUpdateAfterOverviewAnimation) {
  // Run overview animations.
  ui::ScopedAnimationDurationScaleMode regular_animations(
      ui::ScopedAnimationDurationScaleMode::NON_ZERO_DURATION);

  Shelf* shelf = GetPrimaryShelf();
  ASSERT_EQ(SHELF_ALIGNMENT_BOTTOM, shelf->alignment());
  const gfx::Rect bottom_shelf_bounds =
      GetShelfWidget()->GetWindowBoundsInScreen();

  // Change alignment during overview enter animation.
  OverviewController* overview_controller = Shell::Get()->overview_controller();
  {
    OverviewAnimationWaiter waiter;
    overview_controller->StartOverview();
    shelf->SetAlignment(SHELF_ALIGNMENT_LEFT);
    waiter.Wait();
  }
  EXPECT_NE(bottom_shelf_bounds, GetShelfWidget()->GetWindowBoundsInScreen());

  // Change alignment during overview exit animation.
  {
    OverviewAnimationWaiter waiter;
    overview_controller->EndOverview();
    shelf->SetAlignment(SHELF_ALIGNMENT_BOTTOM);
    waiter.Wait();
  }
  EXPECT_EQ(bottom_shelf_bounds, GetShelfWidget()->GetWindowBoundsInScreen());
}

}  // namespace ash
