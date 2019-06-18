// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <vector>

#include "ash/multi_user/multi_user_window_manager_impl.h"
#include "ash/public/cpp/ash_features.h"
#include "ash/public/cpp/multi_user_window_manager.h"
#include "ash/public/cpp/multi_user_window_manager_delegate.h"
#include "ash/shell.h"
#include "ash/test/ash_test_base.h"
#include "ash/window_factory.h"
#include "ash/wm/desks/close_desk_button.h"
#include "ash/wm/desks/desk.h"
#include "ash/wm/desks/desk_mini_view.h"
#include "ash/wm/desks/desks_bar_view.h"
#include "ash/wm/desks/desks_controller.h"
#include "ash/wm/desks/desks_test_util.h"
#include "ash/wm/desks/desks_util.h"
#include "ash/wm/desks/new_desk_button.h"
#include "ash/wm/mru_window_tracker.h"
#include "ash/wm/overview/overview_controller.h"
#include "ash/wm/overview/overview_grid.h"
#include "ash/wm/overview/overview_item.h"
#include "ash/wm/overview/overview_session.h"
#include "ash/wm/splitview/split_view_drag_indicators.h"
#include "ash/wm/tablet_mode/tablet_mode_controller.h"
#include "ash/wm/window_state.h"
#include "ash/wm/window_util.h"
#include "ash/wm/workspace/backdrop_controller.h"
#include "ash/wm/workspace/workspace_layout_manager.h"
#include "ash/wm/workspace_controller.h"
#include "base/stl_util.h"
#include "base/test/scoped_feature_list.h"
#include "ui/aura/client/window_parenting_client.h"
#include "ui/compositor_extra/shadow.h"
#include "ui/events/gesture_detection/gesture_configuration.h"
#include "ui/events/test/event_generator.h"
#include "ui/wm/core/shadow_controller.h"
#include "ui/wm/core/window_util.h"

namespace ash {

namespace {

std::unique_ptr<aura::Window> CreateTransientWindow(
    aura::Window* transient_parent,
    const gfx::Rect& bounds) {
  std::unique_ptr<aura::Window> window =
      window_factory::NewWindow(nullptr, aura::client::WINDOW_TYPE_POPUP);
  window->Init(ui::LAYER_NOT_DRAWN);
  window->SetBounds(bounds);
  ::wm::AddTransientChild(transient_parent, window.get());
  aura::client::ParentWindowWithContext(
      window.get(), transient_parent->GetRootWindow(), bounds);
  window->Show();
  return window;
}

bool DoesActiveDeskContainWindow(aura::Window* window) {
  return DesksController::Get()->active_desk()->windows().contains(window);
}

OverviewGrid* GetOverviewGridForRoot(aura::Window* root) {
  DCHECK(root->IsRootWindow());

  auto* overview_controller = Shell::Get()->overview_controller();
  DCHECK(overview_controller->InOverviewSession());

  return overview_controller->overview_session()->GetGridWithRootWindow(root);
}

void CloseDeskFromMiniView(const DeskMiniView* desk_mini_view,
                           ui::test::EventGenerator* event_generator) {
  DCHECK(desk_mini_view);

  // Move to the center of the mini view so that the close button shows up.
  const gfx::Point mini_view_center =
      desk_mini_view->GetBoundsInScreen().CenterPoint();
  event_generator->MoveMouseTo(mini_view_center);
  EXPECT_TRUE(desk_mini_view->close_desk_button()->GetVisible());
  // Move to the center of the close button and click.
  event_generator->MoveMouseTo(
      desk_mini_view->close_desk_button()->GetBoundsInScreen().CenterPoint());
  event_generator->ClickLeftButton();
}

void ClickOnMiniView(const DeskMiniView* desk_mini_view,
                     ui::test::EventGenerator* event_generator) {
  DCHECK(desk_mini_view);

  const gfx::Point mini_view_center =
      desk_mini_view->GetBoundsInScreen().CenterPoint();
  event_generator->MoveMouseTo(mini_view_center);
  event_generator->ClickLeftButton();
}

void LongGestureTapOnView(const views::View* view,
                          ui::test::EventGenerator* event_generator) {
  event_generator->set_current_screen_location(
      view->GetBoundsInScreen().CenterPoint());
  event_generator->PressTouch();
  ui::GestureConfiguration* gesture_config =
      ui::GestureConfiguration::GetInstance();
  const int long_press_delay_ms = gesture_config->long_press_time_in_ms() +
                                  gesture_config->show_press_delay_in_ms();
  base::RunLoop run_loop;
  base::ThreadTaskRunnerHandle::Get()->PostDelayedTask(
      FROM_HERE, run_loop.QuitClosure(),
      base::TimeDelta::FromMilliseconds(long_press_delay_ms));
  run_loop.Run();
  event_generator->ReleaseTouch();
}

void GestureTapOnView(const views::View* view,
                      ui::test::EventGenerator* event_generator) {
  event_generator->GestureTapAt(view->GetBoundsInScreen().CenterPoint());
}

// If |drop| is false, the dragged |item| won't be dropped; giving the caller
// a chance to do some validations before the item is dropped.
void DragItemToPoint(OverviewItem* item,
                     const gfx::Point& screen_location,
                     ui::test::EventGenerator* event_generator,
                     bool drop = true) {
  DCHECK(item);

  const gfx::Point item_center =
      gfx::ToRoundedPoint(item->target_bounds().CenterPoint());
  event_generator->MoveMouseTo(item_center);
  event_generator->PressLeftButton();
  // Move the mouse by an enough amount in X to engage in the normal drag mode
  // rather than the drag to close mode.
  event_generator->MoveMouseBy(50, 0);
  event_generator->MoveMouseTo(screen_location);
  if (drop)
    event_generator->ReleaseLeftButton();
}

BackdropController* GetDeskBackdropController(const Desk* desk,
                                              aura::Window* root) {
  auto* workspace_controller =
      GetWorkspaceController(desk->GetDeskContainerForRoot(root));
  WorkspaceLayoutManager* layout_manager =
      workspace_controller->layout_manager();
  return layout_manager->backdrop_controller();
}

// Defines an observer to test DesksController notifications.
class TestObserver : public DesksController::Observer {
 public:
  TestObserver() = default;
  ~TestObserver() override = default;

  const std::vector<const Desk*>& desks() const { return desks_; }

  // DesksController::Observer:
  void OnDeskAdded(const Desk* desk) override {
    desks_.emplace_back(desk);
    EXPECT_TRUE(DesksController::Get()->AreDesksBeingModified());
  }
  void OnDeskRemoved(const Desk* desk) override {
    base::Erase(desks_, desk);
    EXPECT_TRUE(DesksController::Get()->AreDesksBeingModified());
  }
  void OnDeskActivationChanged(const Desk* activated,
                               const Desk* deactivated) override {
    EXPECT_TRUE(DesksController::Get()->AreDesksBeingModified());
  }
  void OnDeskSwitchAnimationFinished() override {
    EXPECT_FALSE(DesksController::Get()->AreDesksBeingModified());
  }

 private:
  std::vector<const Desk*> desks_;

  DISALLOW_COPY_AND_ASSIGN(TestObserver);
};

class TestDeskObserver : public Desk::Observer {
 public:
  TestDeskObserver() = default;
  ~TestDeskObserver() override = default;

  int notify_counts() const { return notify_counts_; }

  // Desk::Observer:
  void OnContentChanged() override { ++notify_counts_; }
  void OnDeskDestroyed(const Desk* desk) override {}

 private:
  int notify_counts_ = 0;

  DISALLOW_COPY_AND_ASSIGN(TestDeskObserver);
};

class DesksTest : public AshTestBase {
 public:
  DesksTest() = default;
  ~DesksTest() override = default;

  // AshTestBase:
  void SetUp() override {
    scoped_feature_list_.InitAndEnableFeature(features::kVirtualDesks);

    AshTestBase::SetUp();
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;

  DISALLOW_COPY_AND_ASSIGN(DesksTest);
};

TEST_F(DesksTest, DesksCreationAndRemoval) {
  TestObserver observer;
  auto* controller = DesksController::Get();
  controller->AddObserver(&observer);

  // There's always a default pre-existing desk that cannot be removed.
  EXPECT_EQ(1u, controller->desks().size());
  EXPECT_FALSE(controller->CanRemoveDesks());
  EXPECT_TRUE(controller->CanCreateDesks());

  // Add desks until no longer possible.
  while (controller->CanCreateDesks())
    controller->NewDesk();

  // Expect we've reached the max number of desks, and we've been notified only
  // with the newly created desks.
  EXPECT_EQ(desks_util::kMaxNumberOfDesks, controller->desks().size());
  EXPECT_EQ(desks_util::kMaxNumberOfDesks - 1, observer.desks().size());
  EXPECT_TRUE(controller->CanRemoveDesks());

  // Remove all desks until no longer possible, and expect that there's always
  // one default desk remaining.
  while (controller->CanRemoveDesks())
    controller->RemoveDesk(observer.desks().back());

  EXPECT_EQ(1u, controller->desks().size());
  EXPECT_FALSE(controller->CanRemoveDesks());
  EXPECT_TRUE(controller->CanCreateDesks());
  EXPECT_TRUE(observer.desks().empty());

  controller->RemoveObserver(&observer);
}

TEST_F(DesksTest, DesksBarViewDeskCreation) {
  TestObserver observer;
  auto* controller = DesksController::Get();

  auto* overview_controller = Shell::Get()->overview_controller();
  overview_controller->ToggleOverview();
  EXPECT_TRUE(overview_controller->InOverviewSession());

  const auto* overview_grid =
      GetOverviewGridForRoot(Shell::GetPrimaryRootWindow());

  // Initially the grid is not offset down when there are no desk mini_views
  // once animations are added.
  EXPECT_FALSE(overview_grid->IsDesksBarViewActive());

  const auto* desks_bar_view = overview_grid->GetDesksBarViewForTesting();

  // Since we have a single default desk, there should be no mini_views, and the
  // new desk button is enabled.
  DCHECK(desks_bar_view);
  EXPECT_TRUE(desks_bar_view->mini_views().empty());
  EXPECT_TRUE(desks_bar_view->new_desk_button()->GetEnabled());

  // Click many times on the new desk button and expect only the max number of
  // desks will be created, and the button is no longer enabled.
  const gfx::Point button_center =
      desks_bar_view->new_desk_button()->GetBoundsInScreen().CenterPoint();

  auto* event_generator = GetEventGenerator();
  event_generator->MoveMouseTo(button_center);
  for (size_t i = 0; i < desks_util::kMaxNumberOfDesks + 2; ++i)
    event_generator->ClickLeftButton();

  EXPECT_TRUE(overview_grid->IsDesksBarViewActive());
  EXPECT_EQ(desks_util::kMaxNumberOfDesks, controller->desks().size());
  EXPECT_EQ(controller->desks().size(), desks_bar_view->mini_views().size());
  EXPECT_FALSE(controller->CanCreateDesks());
  EXPECT_TRUE(controller->CanRemoveDesks());
  EXPECT_FALSE(desks_bar_view->new_desk_button()->GetEnabled());

  // Hover over one of the mini_views, and expect that the close button becomes
  // visible.
  const auto* mini_view = desks_bar_view->mini_views().back().get();
  EXPECT_FALSE(mini_view->close_desk_button()->GetVisible());
  const gfx::Point mini_view_center =
      mini_view->GetBoundsInScreen().CenterPoint();
  event_generator->MoveMouseTo(mini_view_center);
  EXPECT_TRUE(mini_view->close_desk_button()->GetVisible());

  // Use the close button to close the desk.
  event_generator->MoveMouseTo(
      mini_view->close_desk_button()->GetBoundsInScreen().CenterPoint());
  event_generator->ClickLeftButton();

  // The new desk button is now enabled again.
  EXPECT_EQ(desks_util::kMaxNumberOfDesks - 1, controller->desks().size());
  EXPECT_EQ(controller->desks().size(), desks_bar_view->mini_views().size());
  EXPECT_TRUE(controller->CanCreateDesks());
  EXPECT_TRUE(desks_bar_view->new_desk_button()->GetEnabled());

  // Exit overview mode and re-enter. Since we have more than one pre-existing
  // desks, their mini_views should be created upon construction of the desks
  // bar.
  overview_controller->ToggleOverview();
  EXPECT_FALSE(overview_controller->InOverviewSession());
  overview_controller->ToggleOverview();
  EXPECT_TRUE(overview_controller->InOverviewSession());

  // Get the new grid and the new desk_bar_view.
  overview_grid =
      overview_controller->overview_session()->GetGridWithRootWindow(
          Shell::GetPrimaryRootWindow());
  EXPECT_TRUE(overview_grid->IsDesksBarViewActive());
  desks_bar_view = overview_grid->GetDesksBarViewForTesting();

  DCHECK(desks_bar_view);
  EXPECT_EQ(controller->desks().size(), desks_bar_view->mini_views().size());
  EXPECT_TRUE(desks_bar_view->new_desk_button()->GetEnabled());
}

TEST_F(DesksTest, DeskActivation) {
  auto* controller = DesksController::Get();
  ASSERT_EQ(1u, controller->desks().size());
  const Desk* desk_1 = controller->desks()[0].get();
  EXPECT_EQ(desk_1, controller->active_desk());
  EXPECT_TRUE(desk_1->is_active());

  auto* root = Shell::GetPrimaryRootWindow();
  EXPECT_TRUE(desk_1->GetDeskContainerForRoot(root)->IsVisible());
  EXPECT_EQ(desks_util::GetActiveDeskContainerForRoot(root),
            desk_1->GetDeskContainerForRoot(root));

  // Create three new desks, and activate one of the middle ones.
  controller->NewDesk();
  controller->NewDesk();
  controller->NewDesk();
  ASSERT_EQ(4u, controller->desks().size());
  const Desk* desk_2 = controller->desks()[1].get();
  const Desk* desk_3 = controller->desks()[2].get();
  const Desk* desk_4 = controller->desks()[3].get();
  EXPECT_FALSE(controller->AreDesksBeingModified());
  ActivateDesk(desk_2);
  EXPECT_FALSE(controller->AreDesksBeingModified());
  EXPECT_EQ(desk_2, controller->active_desk());
  EXPECT_FALSE(desk_1->is_active());
  EXPECT_TRUE(desk_2->is_active());
  EXPECT_FALSE(desk_3->is_active());
  EXPECT_FALSE(desk_4->is_active());
  EXPECT_FALSE(desk_1->GetDeskContainerForRoot(root)->IsVisible());
  EXPECT_TRUE(desk_2->GetDeskContainerForRoot(root)->IsVisible());
  EXPECT_FALSE(desk_3->GetDeskContainerForRoot(root)->IsVisible());
  EXPECT_FALSE(desk_4->GetDeskContainerForRoot(root)->IsVisible());

  // Remove the active desk, which is in the middle, activation should move to
  // the left, so desk 1 should be activated.
  EXPECT_FALSE(controller->AreDesksBeingModified());
  controller->RemoveDesk(desk_2);
  EXPECT_FALSE(controller->AreDesksBeingModified());
  ASSERT_EQ(3u, controller->desks().size());
  EXPECT_EQ(desk_1, controller->active_desk());
  EXPECT_TRUE(desk_1->is_active());
  EXPECT_FALSE(desk_3->is_active());
  EXPECT_FALSE(desk_4->is_active());
  EXPECT_TRUE(desk_1->GetDeskContainerForRoot(root)->IsVisible());
  EXPECT_FALSE(desk_3->GetDeskContainerForRoot(root)->IsVisible());
  EXPECT_FALSE(desk_4->GetDeskContainerForRoot(root)->IsVisible());

  // Remove the active desk, it's the first one on the left, so desk_3 (on the
  // right) will be activated.
  EXPECT_FALSE(controller->AreDesksBeingModified());
  controller->RemoveDesk(desk_1);
  EXPECT_FALSE(controller->AreDesksBeingModified());
  ASSERT_EQ(2u, controller->desks().size());
  EXPECT_EQ(desk_3, controller->active_desk());
  EXPECT_TRUE(desk_3->is_active());
  EXPECT_FALSE(desk_4->is_active());
  EXPECT_TRUE(desk_3->GetDeskContainerForRoot(root)->IsVisible());
  EXPECT_FALSE(desk_4->GetDeskContainerForRoot(root)->IsVisible());
}

// This test makes sure we have coverage for that desk switch animation when run
// with multiple displays.
TEST_F(DesksTest, DeskActivationDualDisplay) {
  UpdateDisplay("600x600,400x500");

  auto* controller = DesksController::Get();
  ASSERT_EQ(1u, controller->desks().size());
  const Desk* desk_1 = controller->desks()[0].get();
  EXPECT_EQ(desk_1, controller->active_desk());
  EXPECT_TRUE(desk_1->is_active());

  // Create three new desks, and activate one of the middle ones.
  controller->NewDesk();
  controller->NewDesk();
  controller->NewDesk();
  ASSERT_EQ(4u, controller->desks().size());
  const Desk* desk_2 = controller->desks()[1].get();
  const Desk* desk_3 = controller->desks()[2].get();
  const Desk* desk_4 = controller->desks()[3].get();
  EXPECT_FALSE(controller->AreDesksBeingModified());
  ActivateDesk(desk_2);
  EXPECT_FALSE(controller->AreDesksBeingModified());
  EXPECT_EQ(desk_2, controller->active_desk());
  EXPECT_FALSE(desk_1->is_active());
  EXPECT_TRUE(desk_2->is_active());
  EXPECT_FALSE(desk_3->is_active());
  EXPECT_FALSE(desk_4->is_active());

  auto roots = Shell::GetAllRootWindows();
  ASSERT_EQ(2u, roots.size());
  EXPECT_FALSE(desk_1->GetDeskContainerForRoot(roots[0])->IsVisible());
  EXPECT_FALSE(desk_1->GetDeskContainerForRoot(roots[1])->IsVisible());
  EXPECT_TRUE(desk_2->GetDeskContainerForRoot(roots[0])->IsVisible());
  EXPECT_TRUE(desk_2->GetDeskContainerForRoot(roots[1])->IsVisible());
  EXPECT_FALSE(desk_3->GetDeskContainerForRoot(roots[0])->IsVisible());
  EXPECT_FALSE(desk_3->GetDeskContainerForRoot(roots[1])->IsVisible());
  EXPECT_FALSE(desk_4->GetDeskContainerForRoot(roots[0])->IsVisible());
  EXPECT_FALSE(desk_4->GetDeskContainerForRoot(roots[1])->IsVisible());
}

TEST_F(DesksTest, TransientWindows) {
  auto* controller = DesksController::Get();
  ASSERT_EQ(1u, controller->desks().size());
  const Desk* desk_1 = controller->desks()[0].get();
  EXPECT_EQ(desk_1, controller->active_desk());
  EXPECT_TRUE(desk_1->is_active());

  // Create two windows, one is a transient child of the other.
  auto win0 = CreateTestWindow(gfx::Rect(0, 0, 250, 100));
  auto win1 = CreateTransientWindow(win0.get(), gfx::Rect(100, 100, 100, 100));

  EXPECT_EQ(2u, desk_1->windows().size());
  EXPECT_TRUE(DoesActiveDeskContainWindow(win0.get()));
  EXPECT_TRUE(DoesActiveDeskContainWindow(win1.get()));

  auto* root = Shell::GetPrimaryRootWindow();
  EXPECT_EQ(desks_util::GetActiveDeskContainerForRoot(root),
            desks_util::GetDeskContainerForContext(win0.get()));
  EXPECT_EQ(desks_util::GetActiveDeskContainerForRoot(root),
            desks_util::GetDeskContainerForContext(win1.get()));

  // Create a new desk and activate it.
  controller->NewDesk();
  const Desk* desk_2 = controller->desks()[1].get();
  EXPECT_TRUE(desk_2->windows().empty());
  ActivateDesk(desk_2);
  EXPECT_FALSE(desk_1->is_active());
  EXPECT_TRUE(desk_2->is_active());

  // Create another transient child of the earlier transient child, and confirm
  // it's tracked in desk_1 (even though desk_2 is the currently active one).
  // This is because the transient parent exists in desk_1.
  auto win2 = CreateTransientWindow(win1.get(), gfx::Rect(100, 100, 50, 50));
  ::wm::AddTransientChild(win1.get(), win2.get());
  EXPECT_EQ(3u, desk_1->windows().size());
  EXPECT_FALSE(DoesActiveDeskContainWindow(win2.get()));

  // Remove the inactive desk 1, and expect that its windows, including
  // transient will move to desk 2.
  controller->RemoveDesk(desk_1);
  EXPECT_EQ(1u, controller->desks().size());
  EXPECT_EQ(desk_2, controller->active_desk());
  EXPECT_EQ(3u, desk_2->windows().size());
  EXPECT_TRUE(DoesActiveDeskContainWindow(win0.get()));
  EXPECT_TRUE(DoesActiveDeskContainWindow(win1.get()));
  EXPECT_TRUE(DoesActiveDeskContainWindow(win2.get()));
}

TEST_F(DesksTest, WindowActivation) {
  // Create three windows.
  auto win0 = CreateTestWindow(gfx::Rect(0, 0, 250, 100));
  auto win1 = CreateTestWindow(gfx::Rect(50, 50, 200, 200));
  auto win2 = CreateTestWindow(gfx::Rect(100, 100, 100, 100));

  EXPECT_TRUE(DoesActiveDeskContainWindow(win0.get()));
  EXPECT_TRUE(DoesActiveDeskContainWindow(win1.get()));
  EXPECT_TRUE(DoesActiveDeskContainWindow(win2.get()));

  // Activate win0 and expects that it remains activated until we switch desks.
  wm::ActivateWindow(win0.get());

  // Create a new desk and activate it. Expect it's not tracking any windows
  // yet.
  auto* controller = DesksController::Get();
  controller->NewDesk();
  ASSERT_EQ(2u, controller->desks().size());
  Desk* desk_1 = controller->desks()[0].get();
  const Desk* desk_2 = controller->desks()[1].get();
  EXPECT_EQ(desk_1, controller->active_desk());
  EXPECT_EQ(3u, desk_1->windows().size());
  EXPECT_TRUE(desk_2->windows().empty());
  EXPECT_EQ(win0.get(), wm::GetActiveWindow());

  // Activate the newly-added desk. Expect that the tracked windows per each
  // desk will remain the same.
  ActivateDesk(desk_2);
  EXPECT_EQ(desk_2, controller->active_desk());
  EXPECT_EQ(3u, desk_1->windows().size());
  EXPECT_TRUE(desk_2->windows().empty());

  // `desk_2` has no windows, so now no window should be active. However,
  // windows on `desk_1` are activateable.
  EXPECT_EQ(nullptr, wm::GetActiveWindow());
  EXPECT_TRUE(wm::CanActivateWindow(win0.get()));
  EXPECT_TRUE(wm::CanActivateWindow(win1.get()));
  EXPECT_TRUE(wm::CanActivateWindow(win2.get()));

  // Create two new windows, they should now go to desk_2.
  auto win3 = CreateTestWindow(gfx::Rect(0, 0, 300, 200));
  auto win4 = CreateTestWindow(gfx::Rect(10, 30, 400, 200));
  wm::ActivateWindow(win3.get());
  EXPECT_EQ(2u, desk_2->windows().size());
  EXPECT_TRUE(DoesActiveDeskContainWindow(win3.get()));
  EXPECT_TRUE(DoesActiveDeskContainWindow(win4.get()));
  EXPECT_FALSE(DoesActiveDeskContainWindow(win0.get()));
  EXPECT_FALSE(DoesActiveDeskContainWindow(win1.get()));
  EXPECT_FALSE(DoesActiveDeskContainWindow(win2.get()));
  EXPECT_EQ(win3.get(), wm::GetActiveWindow());

  // Delete `win0` and expect that `desk_1`'s windows will be updated.
  win0.reset();
  EXPECT_EQ(2u, desk_1->windows().size());
  EXPECT_EQ(2u, desk_2->windows().size());
  // No change in the activation.
  EXPECT_EQ(win3.get(), wm::GetActiveWindow());

  // Switch back to `desk_1`. Now we can activate its windows.
  ActivateDesk(desk_1);
  EXPECT_EQ(desk_1, controller->active_desk());
  EXPECT_TRUE(wm::CanActivateWindow(win1.get()));
  EXPECT_TRUE(wm::CanActivateWindow(win2.get()));
  EXPECT_TRUE(wm::CanActivateWindow(win3.get()));
  EXPECT_TRUE(wm::CanActivateWindow(win4.get()));

  // After `win0` has been deleted, `win2` is next on the MRU list.
  EXPECT_EQ(win2.get(), wm::GetActiveWindow());

  // Remove `desk_2` and expect that its windows will be moved to the active
  // desk.
  TestDeskObserver observer;
  desk_1->AddObserver(&observer);
  controller->RemoveDesk(desk_2);
  EXPECT_EQ(1u, controller->desks().size());
  EXPECT_EQ(desk_1, controller->active_desk());
  EXPECT_EQ(4u, desk_1->windows().size());
  // Even though two new windows have been added to desk_1, observers should be
  // notified only once.
  EXPECT_EQ(1, observer.notify_counts());
  desk_1->RemoveObserver(&observer);
  EXPECT_TRUE(DoesActiveDeskContainWindow(win3.get()));
  EXPECT_TRUE(DoesActiveDeskContainWindow(win4.get()));

  // `desk_2`'s windows moved to `desk_1`, but that should not change the
  // already active window.
  EXPECT_EQ(win2.get(), wm::GetActiveWindow());

  // Moved windows can still be activated.
  EXPECT_TRUE(wm::CanActivateWindow(win3.get()));
  EXPECT_TRUE(wm::CanActivateWindow(win4.get()));
}

TEST_F(DesksTest, ActivateDeskFromOverview) {
  auto* controller = DesksController::Get();

  // Create three desks other than the default initial desk.
  controller->NewDesk();
  controller->NewDesk();
  controller->NewDesk();
  ASSERT_EQ(4u, controller->desks().size());

  // Create two windows on desk_1.
  auto win0 = CreateTestWindow(gfx::Rect(0, 0, 250, 100));
  auto win1 = CreateTestWindow(gfx::Rect(50, 50, 200, 200));
  wm::ActivateWindow(win1.get());
  EXPECT_EQ(win1.get(), wm::GetActiveWindow());

  // Enter overview mode, and expect the desk bar is shown with exactly four
  // desks mini views, and there are exactly two windows in the overview mode
  // grid.
  auto* overview_controller = Shell::Get()->overview_controller();
  overview_controller->ToggleOverview();
  EXPECT_TRUE(overview_controller->InOverviewSession());
  const auto* overview_grid =
      GetOverviewGridForRoot(Shell::GetPrimaryRootWindow());
  const auto* desks_bar_view = overview_grid->GetDesksBarViewForTesting();
  ASSERT_TRUE(desks_bar_view);
  ASSERT_EQ(4u, desks_bar_view->mini_views().size());
  EXPECT_EQ(2u, overview_grid->window_list().size());

  // Activate desk_4 (last one on the right) by clicking on its mini view.
  const Desk* desk_4 = controller->desks()[3].get();
  EXPECT_FALSE(desk_4->is_active());
  auto* mini_view = desks_bar_view->mini_views().back().get();
  EXPECT_EQ(desk_4, mini_view->desk());
  EXPECT_FALSE(mini_view->close_desk_button()->GetVisible());
  const gfx::Point mini_view_center =
      mini_view->GetBoundsInScreen().CenterPoint();
  auto* event_generator = GetEventGenerator();
  event_generator->MoveMouseTo(mini_view_center);
  DeskSwitchAnimationWaiter waiter;
  event_generator->ClickLeftButton();
  waiter.Wait();

  // Expect that desk_4 is now active, and overview mode exited.
  EXPECT_TRUE(desk_4->is_active());
  EXPECT_FALSE(overview_controller->InOverviewSession());
  // Exiting overview mode should not restore focus to a window on a
  // now-inactive desk. Run a loop since the overview session is destroyed async
  // and until that happens, focus will be on the dummy
  // "OverviewModeFocusedWidget".
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(nullptr, wm::GetActiveWindow());

  // Create one window in desk_4 and enter overview mode. Expect the grid is
  // showing exactly one window.
  auto win2 = CreateTestWindow(gfx::Rect(50, 50, 200, 200));
  wm::ActivateWindow(win2.get());
  overview_controller->ToggleOverview();
  EXPECT_TRUE(overview_controller->InOverviewSession());
  overview_grid = GetOverviewGridForRoot(Shell::GetPrimaryRootWindow());
  EXPECT_EQ(1u, overview_grid->window_list().size());

  // When exiting overview mode without changing desks, the focus should be
  // restored to the same window.
  overview_controller->ToggleOverview();
  EXPECT_FALSE(overview_controller->InOverviewSession());
  // Run a loop since the overview session is destroyed async and until that
  // happens, focus will be on the dummy "OverviewModeFocusedWidget".
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(win2.get(), wm::GetActiveWindow());
}

// This test makes sure we have coverage for that desk switch animation when run
// with multiple displays while overview mode is active.
TEST_F(DesksTest, ActivateDeskFromOverviewDualDisplay) {
  UpdateDisplay("600x600,400x500");

  auto* controller = DesksController::Get();

  // Create three desks other than the default initial desk.
  controller->NewDesk();
  controller->NewDesk();
  controller->NewDesk();
  ASSERT_EQ(4u, controller->desks().size());

  // Enter overview mode.
  auto* overview_controller = Shell::Get()->overview_controller();
  overview_controller->ToggleOverview();
  EXPECT_TRUE(overview_controller->InOverviewSession());

  auto roots = Shell::GetAllRootWindows();
  ASSERT_EQ(2u, roots.size());
  // Use secondary display grid.
  const auto* overview_grid = GetOverviewGridForRoot(roots[1]);
  const auto* desks_bar_view = overview_grid->GetDesksBarViewForTesting();
  ASSERT_TRUE(desks_bar_view);
  ASSERT_EQ(4u, desks_bar_view->mini_views().size());

  // Activate desk_4 (last one on the right) by clicking on its mini view.
  const Desk* desk_4 = controller->desks()[3].get();
  EXPECT_FALSE(desk_4->is_active());
  const auto* mini_view = desks_bar_view->mini_views().back().get();
  const gfx::Point mini_view_center =
      mini_view->GetBoundsInScreen().CenterPoint();
  auto* event_generator = GetEventGenerator();
  event_generator->MoveMouseTo(mini_view_center);
  DeskSwitchAnimationWaiter waiter;
  event_generator->ClickLeftButton();
  waiter.Wait();

  // Expect that desk_4 is now active, and overview mode exited.
  EXPECT_TRUE(desk_4->is_active());
  EXPECT_FALSE(overview_controller->InOverviewSession());
}

TEST_F(DesksTest, RemoveInactiveDeskFromOverview) {
  auto* controller = DesksController::Get();

  // Create three desks other than the default initial desk.
  controller->NewDesk();
  controller->NewDesk();
  controller->NewDesk();
  ASSERT_EQ(4u, controller->desks().size());

  // Create two windows on desk_1.
  auto win0 = CreateTestWindow(gfx::Rect(0, 0, 250, 100));
  auto win1 = CreateTestWindow(gfx::Rect(50, 50, 200, 200));
  wm::ActivateWindow(win0.get());
  EXPECT_EQ(win0.get(), wm::GetActiveWindow());

  // Active desk_4 and enter overview mode. Expect that the grid is currently
  // empty.
  Desk* desk_4 = controller->desks()[3].get();
  ActivateDesk(desk_4);
  auto* overview_controller = Shell::Get()->overview_controller();
  overview_controller->ToggleOverview();
  EXPECT_TRUE(overview_controller->InOverviewSession());
  const auto* overview_grid =
      GetOverviewGridForRoot(Shell::GetPrimaryRootWindow());
  EXPECT_TRUE(overview_grid->window_list().empty());

  // Remove desk_1 using the close button on its mini view. desk_1 is currently
  // inactive. Its windows should be moved to desk_4 and added to the overview
  // grid in the MRU order (win0, and win1).
  const auto* desks_bar_view = overview_grid->GetDesksBarViewForTesting();
  ASSERT_TRUE(desks_bar_view);
  ASSERT_EQ(4u, desks_bar_view->mini_views().size());
  Desk* desk_1 = controller->desks()[0].get();
  auto* mini_view = desks_bar_view->mini_views().front().get();
  EXPECT_EQ(desk_1, mini_view->desk());

  // Setup observers of both the active and inactive desks to make sure
  // refreshing the mini_view is requested only *once* for the active desk, and
  // never for the to-be-removed inactive desk.
  TestDeskObserver desk_4_observer;
  desk_4->AddObserver(&desk_4_observer);
  TestDeskObserver desk_1_observer;
  desk_1->AddObserver(&desk_1_observer);

  CloseDeskFromMiniView(mini_view, GetEventGenerator());

  EXPECT_EQ(0, desk_1_observer.notify_counts());
  EXPECT_EQ(1, desk_4_observer.notify_counts());

  ASSERT_EQ(3u, desks_bar_view->mini_views().size());
  EXPECT_TRUE(overview_controller->InOverviewSession());
  ASSERT_EQ(2u, overview_grid->window_list().size());
  EXPECT_TRUE(overview_grid->GetOverviewItemContaining(win0.get()));
  EXPECT_TRUE(overview_grid->GetOverviewItemContaining(win1.get()));
  EXPECT_EQ(overview_grid->GetOverviewItemContaining(win0.get()),
            overview_grid->window_list()[0].get());
  EXPECT_EQ(overview_grid->GetOverviewItemContaining(win1.get()),
            overview_grid->window_list()[1].get());

  // Make sure overview mode remains active.
  base::RunLoop().RunUntilIdle();
  EXPECT_TRUE(overview_controller->InOverviewSession());

  // Removing a desk with no windows should not result in any new mini_views
  // updates.
  mini_view = desks_bar_view->mini_views().front().get();
  EXPECT_TRUE(mini_view->desk()->windows().empty());
  CloseDeskFromMiniView(mini_view, GetEventGenerator());
  EXPECT_EQ(1, desk_4_observer.notify_counts());

  // Exiting overview mode should not cause any mini_views refreshes, since the
  // destroyed overview-specific windows do not show up in the mini_view.
  overview_controller->ToggleOverview();
  EXPECT_FALSE(overview_controller->InOverviewSession());
  EXPECT_EQ(1, desk_4_observer.notify_counts());
  desk_4->RemoveObserver(&desk_4_observer);
}

TEST_F(DesksTest, RemoveActiveDeskFromOverview) {
  auto* controller = DesksController::Get();

  // Create one desk other than the default initial desk.
  controller->NewDesk();
  ASSERT_EQ(2u, controller->desks().size());

  // Create two windows on desk_1.
  Desk* desk_1 = controller->desks()[0].get();
  auto win0 = CreateTestWindow(gfx::Rect(0, 0, 250, 100));
  auto win1 = CreateTestWindow(gfx::Rect(50, 50, 200, 200));
  wm::ActivateWindow(win0.get());
  EXPECT_EQ(win0.get(), wm::GetActiveWindow());

  // Activate desk_2 and create one more window.
  Desk* desk_2 = controller->desks()[1].get();
  ActivateDesk(desk_2);
  auto win2 = CreateTestWindow(gfx::Rect(50, 50, 200, 200));
  wm::ActivateWindow(win2.get());
  EXPECT_EQ(win2.get(), wm::GetActiveWindow());

  // Enter overview mode, and remove desk_2 from its mini-view close button.
  auto* overview_controller = Shell::Get()->overview_controller();
  overview_controller->ToggleOverview();
  EXPECT_TRUE(overview_controller->InOverviewSession());
  const auto* overview_grid =
      GetOverviewGridForRoot(Shell::GetPrimaryRootWindow());
  EXPECT_EQ(1u, overview_grid->window_list().size());
  const auto* desks_bar_view = overview_grid->GetDesksBarViewForTesting();
  ASSERT_TRUE(desks_bar_view);
  ASSERT_EQ(2u, desks_bar_view->mini_views().size());
  auto* mini_view = desks_bar_view->mini_views().back().get();
  EXPECT_EQ(desk_2, mini_view->desk());

  // Setup observers of both the active and inactive desks to make sure
  // refreshing the mini_view is requested only *once* for the soon-to-be active
  // desk_1, and never for the to-be-removed currently active desk_2.
  TestDeskObserver desk_1_observer;
  desk_1->AddObserver(&desk_1_observer);
  TestDeskObserver desk_2_observer;
  desk_2->AddObserver(&desk_2_observer);

  CloseDeskFromMiniView(mini_view, GetEventGenerator());

  EXPECT_EQ(1, desk_1_observer.notify_counts());
  EXPECT_EQ(0, desk_2_observer.notify_counts());

  // desk_1 will become active, and windows from desk_2 and desk_1 will merge
  // and added in the overview grid in the order of MRU.
  ASSERT_EQ(1u, controller->desks().size());
  ASSERT_EQ(1u, desks_bar_view->mini_views().size());
  EXPECT_TRUE(desk_1->is_active());
  EXPECT_TRUE(overview_controller->InOverviewSession());
  EXPECT_EQ(3u, overview_grid->window_list().size());
  EXPECT_TRUE(overview_grid->GetOverviewItemContaining(win0.get()));
  EXPECT_TRUE(overview_grid->GetOverviewItemContaining(win1.get()));
  EXPECT_TRUE(overview_grid->GetOverviewItemContaining(win2.get()));

  // The MRU order is {win2, win0, win1}.
  EXPECT_EQ(overview_grid->GetOverviewItemContaining(win2.get()),
            overview_grid->window_list()[0].get());
  EXPECT_EQ(overview_grid->GetOverviewItemContaining(win0.get()),
            overview_grid->window_list()[1].get());
  EXPECT_EQ(overview_grid->GetOverviewItemContaining(win1.get()),
            overview_grid->window_list()[2].get());

  // Make sure overview mode remains active.
  base::RunLoop().RunUntilIdle();
  EXPECT_TRUE(overview_controller->InOverviewSession());

  // Exiting overview mode should not cause any mini_views refreshes, since the
  // destroyed overview-specific windows do not show up in the mini_view.
  overview_controller->ToggleOverview();
  EXPECT_FALSE(overview_controller->InOverviewSession());
  EXPECT_EQ(1, desk_1_observer.notify_counts());
  desk_1->RemoveObserver(&desk_1_observer);
}

TEST_F(DesksTest, ActivateActiveDeskFromOverview) {
  auto* controller = DesksController::Get();

  // Create one more desk other than the default initial desk, so the desks bar
  // shows up in overview mode.
  controller->NewDesk();
  ASSERT_EQ(2u, controller->desks().size());

  // Enter overview mode, and click on `desk_1`'s mini_view, and expect that
  // overview mode exits since this is the already active desk.
  auto* overview_controller = Shell::Get()->overview_controller();
  overview_controller->ToggleOverview();
  EXPECT_TRUE(overview_controller->InOverviewSession());
  const auto* overview_grid =
      GetOverviewGridForRoot(Shell::GetPrimaryRootWindow());
  const auto* desks_bar_view = overview_grid->GetDesksBarViewForTesting();
  const Desk* desk_1 = controller->desks()[0].get();
  const auto* mini_view = desks_bar_view->mini_views().front().get();
  ClickOnMiniView(mini_view, GetEventGenerator());
  EXPECT_FALSE(overview_controller->InOverviewSession());
  EXPECT_TRUE(desk_1->is_active());
  EXPECT_EQ(desk_1, controller->active_desk());
}

TEST_F(DesksTest, MinimizedWindow) {
  auto* controller = DesksController::Get();
  controller->NewDesk();
  ASSERT_EQ(2u, controller->desks().size());
  const Desk* desk_1 = controller->desks()[0].get();
  const Desk* desk_2 = controller->desks()[1].get();

  auto win0 = CreateTestWindow(gfx::Rect(0, 0, 250, 100));
  wm::ActivateWindow(win0.get());
  EXPECT_EQ(win0.get(), wm::GetActiveWindow());

  auto* window_state = wm::GetWindowState(win0.get());
  window_state->Minimize();
  EXPECT_TRUE(window_state->IsMinimized());

  // Minimized windows on the active desk show up in the MRU list...
  EXPECT_EQ(1u, Shell::Get()
                    ->mru_window_tracker()
                    ->BuildMruWindowList(kActiveDesk)
                    .size());

  ActivateDesk(desk_2);

  // ... But they don't once their desk is inactive.
  EXPECT_TRUE(Shell::Get()
                  ->mru_window_tracker()
                  ->BuildMruWindowList(kActiveDesk)
                  .empty());

  // Switching back to their desk should neither activate them nor unminimize
  // them.
  ActivateDesk(desk_1);
  EXPECT_TRUE(window_state->IsMinimized());
  EXPECT_NE(win0.get(), wm::GetActiveWindow());
}

TEST_F(DesksTest, DragWindowToDesk) {
  auto* controller = DesksController::Get();
  controller->NewDesk();
  ASSERT_EQ(2u, controller->desks().size());
  const Desk* desk_1 = controller->desks()[0].get();
  const Desk* desk_2 = controller->desks()[1].get();

  auto window = CreateTestWindow(gfx::Rect(0, 0, 250, 100));
  wm::ActivateWindow(window.get());
  EXPECT_EQ(window.get(), wm::GetActiveWindow());

  ui::Shadow* shadow = ::wm::ShadowController::GetShadowForWindow(window.get());
  ASSERT_TRUE(shadow);
  ASSERT_TRUE(shadow->layer());
  EXPECT_TRUE(shadow->layer()->GetTargetVisibility());

  auto* overview_controller = Shell::Get()->overview_controller();
  overview_controller->ToggleOverview();
  EXPECT_TRUE(overview_controller->InOverviewSession());
  auto* overview_grid = GetOverviewGridForRoot(Shell::GetPrimaryRootWindow());
  EXPECT_EQ(1u, overview_grid->size());

  // While in overview mode, the window's shadow is hidden.
  EXPECT_FALSE(shadow->layer()->GetTargetVisibility());

  auto* overview_session = overview_controller->overview_session();
  auto* overview_item =
      overview_session->GetOverviewItemForWindow(window.get());
  ASSERT_TRUE(overview_item);
  const gfx::RectF target_bounds_before_drag = overview_item->target_bounds();

  const auto* desks_bar_view = overview_grid->GetDesksBarViewForTesting();
  ASSERT_TRUE(desks_bar_view);
  ASSERT_EQ(2u, desks_bar_view->mini_views().size());
  auto* desk_1_mini_view = desks_bar_view->mini_views()[0].get();
  EXPECT_EQ(desk_1, desk_1_mini_view->desk());
  // Drag it and drop it on its same desk's mini_view. Nothing happens, it
  // should be returned back to its original target bounds.
  DragItemToPoint(overview_item,
                  desk_1_mini_view->GetBoundsInScreen().CenterPoint(),
                  GetEventGenerator());
  EXPECT_TRUE(overview_controller->InOverviewSession());
  EXPECT_EQ(1u, overview_grid->size());
  EXPECT_EQ(target_bounds_before_drag, overview_item->target_bounds());
  EXPECT_TRUE(DoesActiveDeskContainWindow(window.get()));

  // Now drag it to desk_2's mini_view. The overview grid should now show the
  // "no-windows" widget, and the window should move to desk_2.
  auto* desk_2_mini_view = desks_bar_view->mini_views()[1].get();
  EXPECT_EQ(desk_2, desk_2_mini_view->desk());
  DragItemToPoint(overview_item,
                  desk_2_mini_view->GetBoundsInScreen().CenterPoint(),
                  GetEventGenerator());
  EXPECT_TRUE(overview_controller->InOverviewSession());
  EXPECT_TRUE(overview_grid->empty());
  EXPECT_FALSE(DoesActiveDeskContainWindow(window.get()));
  EXPECT_TRUE(overview_session->no_windows_widget_for_testing());
  EXPECT_TRUE(desk_2->windows().contains(window.get()));
  EXPECT_FALSE(overview_grid->drop_target_widget());

  // After the window is dropped onto another desk, its shadow should be
  // restored properly.
  EXPECT_TRUE(shadow->layer()->GetTargetVisibility());
}

TEST_F(DesksTest, DragMinimizedWindowToDesk) {
  auto* controller = DesksController::Get();
  controller->NewDesk();
  ASSERT_EQ(2u, controller->desks().size());
  const Desk* desk_2 = controller->desks()[1].get();

  auto window = CreateTestWindow(gfx::Rect(0, 0, 250, 100));
  wm::ActivateWindow(window.get());
  EXPECT_EQ(window.get(), wm::GetActiveWindow());

  // Minimize the window before entering Overview Mode.
  auto* window_state = wm::GetWindowState(window.get());
  window_state->Minimize();
  ASSERT_TRUE(window_state->IsMinimized());

  auto* overview_controller = Shell::Get()->overview_controller();
  overview_controller->ToggleOverview();
  EXPECT_TRUE(overview_controller->InOverviewSession());
  auto* overview_grid = GetOverviewGridForRoot(Shell::GetPrimaryRootWindow());
  auto* overview_session = overview_controller->overview_session();
  auto* overview_item =
      overview_session->GetOverviewItemForWindow(window.get());
  ASSERT_TRUE(overview_item);
  const auto* desks_bar_view = overview_grid->GetDesksBarViewForTesting();
  ASSERT_TRUE(desks_bar_view);
  ASSERT_EQ(2u, desks_bar_view->mini_views().size());

  // Drag the window to desk_2's mini_view and activate desk_2. Expect that the
  // window will be in an unminimized state and all its visibility and layer
  // opacity attributes are correct.
  auto* desk_2_mini_view = desks_bar_view->mini_views()[1].get();
  EXPECT_EQ(desk_2, desk_2_mini_view->desk());
  DragItemToPoint(overview_item,
                  desk_2_mini_view->GetBoundsInScreen().CenterPoint(),
                  GetEventGenerator());
  EXPECT_TRUE(overview_controller->InOverviewSession());
  EXPECT_TRUE(overview_grid->empty());
  EXPECT_TRUE(desk_2->windows().contains(window.get()));
  EXPECT_FALSE(overview_grid->drop_target_widget());
  DeskSwitchAnimationWaiter waiter;
  ClickOnMiniView(desk_2_mini_view, GetEventGenerator());
  waiter.Wait();
  EXPECT_FALSE(overview_controller->InOverviewSession());
  EXPECT_TRUE(desk_2->is_active());

  EXPECT_FALSE(window_state->IsMinimized());
  EXPECT_TRUE(window->IsVisible());
  EXPECT_TRUE(window->layer()->GetTargetVisibility());
  EXPECT_EQ(1.f, window->layer()->GetTargetOpacity());
}

TEST_F(DesksTest, DragWindowToNonMiniViewPoints) {
  auto* controller = DesksController::Get();
  controller->NewDesk();
  ASSERT_EQ(2u, controller->desks().size());

  auto window = CreateTestWindow(gfx::Rect(0, 0, 250, 100));
  wm::ActivateWindow(window.get());
  EXPECT_EQ(window.get(), wm::GetActiveWindow());

  auto* overview_controller = Shell::Get()->overview_controller();
  overview_controller->ToggleOverview();
  EXPECT_TRUE(overview_controller->InOverviewSession());
  const auto* overview_grid =
      GetOverviewGridForRoot(Shell::GetPrimaryRootWindow());
  EXPECT_EQ(1u, overview_grid->size());

  auto* overview_session = overview_controller->overview_session();
  auto* overview_item =
      overview_session->GetOverviewItemForWindow(window.get());
  ASSERT_TRUE(overview_item);
  const gfx::RectF target_bounds_before_drag = overview_item->target_bounds();

  const auto* desks_bar_view = overview_grid->GetDesksBarViewForTesting();
  ASSERT_TRUE(desks_bar_view);

  // Drag it and drop it on the new desk button. Nothing happens, it should be
  // returned back to its original target bounds.
  DragItemToPoint(
      overview_item,
      desks_bar_view->new_desk_button()->GetBoundsInScreen().CenterPoint(),
      GetEventGenerator());
  EXPECT_TRUE(overview_controller->InOverviewSession());
  EXPECT_EQ(1u, overview_grid->size());
  EXPECT_EQ(target_bounds_before_drag, overview_item->target_bounds());
  EXPECT_TRUE(DoesActiveDeskContainWindow(window.get()));

  // Drag it and drop it on the bottom right corner of the display. Also,
  // nothing should happen.
  DragItemToPoint(overview_item,
                  window->GetRootWindow()->GetBoundsInScreen().bottom_right(),
                  GetEventGenerator());
  EXPECT_TRUE(overview_controller->InOverviewSession());
  EXPECT_EQ(1u, overview_grid->size());
  EXPECT_EQ(target_bounds_before_drag, overview_item->target_bounds());
  EXPECT_TRUE(DoesActiveDeskContainWindow(window.get()));
}

TEST_F(DesksTest, MruWindowTracker) {
  // Create two desks with two windows in each.
  auto win0 = CreateTestWindow(gfx::Rect(0, 0, 250, 100));
  auto win1 = CreateTestWindow(gfx::Rect(50, 50, 200, 200));
  auto* controller = DesksController::Get();
  controller->NewDesk();
  ASSERT_EQ(2u, controller->desks().size());
  const Desk* desk_2 = controller->desks()[1].get();
  ActivateDesk(desk_2);
  EXPECT_EQ(desk_2, controller->active_desk());
  auto win2 = CreateTestWindow(gfx::Rect(0, 0, 300, 200));
  auto win3 = CreateTestWindow(gfx::Rect(10, 30, 400, 200));

  // Build active desk's MRU window list.
  auto* mru_window_tracker = Shell::Get()->mru_window_tracker();
  auto window_list = mru_window_tracker->BuildWindowForCycleList(kActiveDesk);
  ASSERT_EQ(2u, window_list.size());
  EXPECT_EQ(win3.get(), window_list[0]);
  EXPECT_EQ(win2.get(), window_list[1]);

  // Build the global MRU window list.
  window_list = mru_window_tracker->BuildWindowForCycleList(kAllDesks);
  ASSERT_EQ(4u, window_list.size());
  EXPECT_EQ(win3.get(), window_list[0]);
  EXPECT_EQ(win2.get(), window_list[1]);
  EXPECT_EQ(win1.get(), window_list[2]);
  EXPECT_EQ(win0.get(), window_list[3]);

  // Switch back to desk_1 and test both MRU list types.
  Desk* desk_1 = controller->desks()[0].get();
  ActivateDesk(desk_1);
  window_list = mru_window_tracker->BuildWindowForCycleList(kActiveDesk);
  ASSERT_EQ(2u, window_list.size());
  EXPECT_EQ(win1.get(), window_list[0]);
  EXPECT_EQ(win0.get(), window_list[1]);
  // TODO(afakhry): Check with UX if we should favor active desk's windows in
  // the global MRU list (i.e. put them first in the list).
  window_list = mru_window_tracker->BuildWindowForCycleList(kAllDesks);
  ASSERT_EQ(4u, window_list.size());
  EXPECT_EQ(win1.get(), window_list[0]);
  EXPECT_EQ(win3.get(), window_list[1]);
  EXPECT_EQ(win2.get(), window_list[2]);
  EXPECT_EQ(win0.get(), window_list[3]);
}

TEST_F(DesksTest, NextActivatable) {
  // Create two desks with two windows in each.
  auto win0 = CreateTestWindow(gfx::Rect(0, 0, 250, 100));
  auto win1 = CreateTestWindow(gfx::Rect(50, 50, 200, 200));
  auto* controller = DesksController::Get();
  controller->NewDesk();
  ASSERT_EQ(2u, controller->desks().size());
  const Desk* desk_2 = controller->desks()[1].get();
  ActivateDesk(desk_2);
  EXPECT_EQ(desk_2, controller->active_desk());
  auto win2 = CreateTestWindow(gfx::Rect(0, 0, 300, 200));
  auto win3 = CreateTestWindow(gfx::Rect(10, 30, 400, 200));
  EXPECT_EQ(win3.get(), wm::GetActiveWindow());

  // When deactivating a window, the next activatable window should be on the
  // same desk.
  wm::DeactivateWindow(win3.get());
  EXPECT_EQ(win2.get(), wm::GetActiveWindow());
  wm::DeactivateWindow(win2.get());
  EXPECT_EQ(win3.get(), wm::GetActiveWindow());

  // Similarly for desk_1.
  Desk* desk_1 = controller->desks()[0].get();
  ActivateDesk(desk_1);
  EXPECT_EQ(win1.get(), wm::GetActiveWindow());
  win1.reset();
  EXPECT_EQ(win0.get(), wm::GetActiveWindow());
  win0.reset();
  EXPECT_EQ(nullptr, wm::GetActiveWindow());
}

TEST_F(DesksTest, TabletModeBackdrops) {
  auto* controller = DesksController::Get();
  controller->NewDesk();
  ASSERT_EQ(2u, controller->desks().size());
  const Desk* desk_1 = controller->desks()[0].get();
  const Desk* desk_2 = controller->desks()[1].get();

  auto window = CreateTestWindow(gfx::Rect(0, 0, 250, 100));
  wm::ActivateWindow(window.get());
  EXPECT_EQ(window.get(), wm::GetActiveWindow());

  // Enter tablet mode and expect that the backdrop is created only for desk_1,
  // since it's the one that has a window in it.
  // Avoid TabletModeController::OnGetSwitchStates() from disabling tablet mode.
  base::RunLoop().RunUntilIdle();
  Shell::Get()->tablet_mode_controller()->SetEnabledForTest(true);
  auto* desk_1_backdrop_controller =
      GetDeskBackdropController(desk_1, Shell::GetPrimaryRootWindow());
  auto* desk_2_backdrop_controller =
      GetDeskBackdropController(desk_2, Shell::GetPrimaryRootWindow());
  ASSERT_TRUE(desk_1_backdrop_controller->backdrop_window());
  EXPECT_TRUE(desk_1_backdrop_controller->backdrop_window()->IsVisible());
  EXPECT_FALSE(desk_2_backdrop_controller->backdrop_window());

  // Enter overview and expect that the backdrop is still present for desk_1 but
  // hidden.
  auto* overview_controller = Shell::Get()->overview_controller();
  overview_controller->ToggleOverview();
  EXPECT_TRUE(overview_controller->InOverviewSession());
  ASSERT_TRUE(desk_1_backdrop_controller->backdrop_window());
  EXPECT_FALSE(desk_1_backdrop_controller->backdrop_window()->IsVisible());

  auto* overview_grid = GetOverviewGridForRoot(Shell::GetPrimaryRootWindow());
  EXPECT_EQ(1u, overview_grid->size());

  auto* overview_session = overview_controller->overview_session();
  auto* overview_item =
      overview_session->GetOverviewItemForWindow(window.get());
  ASSERT_TRUE(overview_item);
  const auto* desks_bar_view = overview_grid->GetDesksBarViewForTesting();
  ASSERT_TRUE(desks_bar_view);

  // Now drag it to desk_2's mini_view, so that it moves to desk_2. Expect that
  // desk_1's backdrop is destroyed, while created (but still hidden) for
  // desk_2.
  auto* desk_2_mini_view = desks_bar_view->mini_views()[1].get();
  EXPECT_EQ(desk_2, desk_2_mini_view->desk());
  DragItemToPoint(overview_item,
                  desk_2_mini_view->GetBoundsInScreen().CenterPoint(),
                  GetEventGenerator());
  EXPECT_TRUE(overview_controller->InOverviewSession());
  EXPECT_TRUE(desk_2->windows().contains(window.get()));
  EXPECT_FALSE(desk_1_backdrop_controller->backdrop_window());
  ASSERT_TRUE(desk_2_backdrop_controller->backdrop_window());
  EXPECT_FALSE(desk_2_backdrop_controller->backdrop_window()->IsVisible());

  // Exit overview, and expect that desk_2's backdrop remains hidden since the
  // desk is not activated yet.
  overview_controller->ToggleOverview(
      OverviewSession::EnterExitOverviewType::kImmediateExit);
  EXPECT_FALSE(overview_controller->InOverviewSession());
  EXPECT_FALSE(desk_1_backdrop_controller->backdrop_window());
  ASSERT_TRUE(desk_2_backdrop_controller->backdrop_window());
  EXPECT_FALSE(desk_2_backdrop_controller->backdrop_window()->IsVisible());

  // Activate desk_2 and expect that its backdrop is now visible.
  ActivateDesk(desk_2);
  EXPECT_FALSE(desk_1_backdrop_controller->backdrop_window());
  ASSERT_TRUE(desk_2_backdrop_controller->backdrop_window());
  EXPECT_TRUE(desk_2_backdrop_controller->backdrop_window()->IsVisible());

  // No backdrops after exiting tablet mode.
  Shell::Get()->tablet_mode_controller()->SetEnabledForTest(false);
  EXPECT_FALSE(desk_1_backdrop_controller->backdrop_window());
  EXPECT_FALSE(desk_2_backdrop_controller->backdrop_window());
}

TEST_F(DesksTest, TabletModeDesksCreationRemovalCycle) {
  auto window = CreateTestWindow(gfx::Rect(0, 0, 250, 100));
  wm::ActivateWindow(window.get());
  EXPECT_EQ(window.get(), wm::GetActiveWindow());

  // Enter tablet mode. Avoid TabletModeController::OnGetSwitchStates() from
  // disabling tablet mode.
  base::RunLoop().RunUntilIdle();
  Shell::Get()->tablet_mode_controller()->SetEnabledForTest(true);

  auto* overview_controller = Shell::Get()->overview_controller();
  overview_controller->ToggleOverview();
  EXPECT_TRUE(overview_controller->InOverviewSession());
  auto* desks_controller = DesksController::Get();

  // Create and remove desks in a cycle while in overview mode. Expect as the
  // containers are reused for new desks, their backdrop state are always
  // correct, and there are no crashes as desks are removed.
  for (size_t i = 0; i < 2 * desks_util::kMaxNumberOfDesks; ++i) {
    desks_controller->NewDesk();
    ASSERT_EQ(2u, desks_controller->desks().size());
    const Desk* desk_1 = desks_controller->desks()[0].get();
    const Desk* desk_2 = desks_controller->desks()[1].get();
    auto* desk_1_backdrop_controller =
        GetDeskBackdropController(desk_1, Shell::GetPrimaryRootWindow());
    auto* desk_2_backdrop_controller =
        GetDeskBackdropController(desk_2, Shell::GetPrimaryRootWindow());
    {
      SCOPED_TRACE("Check backdrops after desk creation");
      ASSERT_TRUE(desk_1_backdrop_controller->backdrop_window());
      EXPECT_FALSE(desk_1_backdrop_controller->backdrop_window()->IsVisible());
      EXPECT_FALSE(desk_2_backdrop_controller->backdrop_window());
    }
    // Remove the active desk, and expect that now desk_2 should have a hidden
    // backdrop, while the container of the removed desk_1 should have none.
    desks_controller->RemoveDesk(desk_1);
    {
      SCOPED_TRACE("Check backdrops after desk removal");
      EXPECT_TRUE(desk_2->is_active());
      EXPECT_TRUE(DoesActiveDeskContainWindow(window.get()));
      EXPECT_FALSE(desk_1_backdrop_controller->backdrop_window());
      ASSERT_TRUE(desk_2_backdrop_controller->backdrop_window());
      EXPECT_FALSE(desk_2_backdrop_controller->backdrop_window()->IsVisible());
    }
  }
}

TEST_F(DesksTest, MiniViewsTouchGestures) {
  auto* controller = DesksController::Get();
  controller->NewDesk();
  controller->NewDesk();
  ASSERT_EQ(3u, controller->desks().size());
  auto* overview_controller = Shell::Get()->overview_controller();
  overview_controller->ToggleOverview();
  EXPECT_TRUE(overview_controller->InOverviewSession());
  const auto* overview_grid =
      GetOverviewGridForRoot(Shell::GetPrimaryRootWindow());
  const auto* desks_bar_view = overview_grid->GetDesksBarViewForTesting();
  ASSERT_TRUE(desks_bar_view);
  ASSERT_EQ(3u, desks_bar_view->mini_views().size());
  auto* desk_1_mini_view = desks_bar_view->mini_views()[0].get();
  auto* desk_2_mini_view = desks_bar_view->mini_views()[1].get();
  auto* desk_3_mini_view = desks_bar_view->mini_views()[2].get();

  // Override the long-tap delays.
  ui::GestureConfiguration* gesture_config =
      ui::GestureConfiguration::GetInstance();
  gesture_config->set_long_press_time_in_ms(50);
  gesture_config->set_show_press_delay_in_ms(50);

  // Long gesture tapping on one mini_view shows its close button, and hides
  // those of other mini_views.
  auto* event_generator = GetEventGenerator();
  LongGestureTapOnView(desk_1_mini_view, event_generator);
  EXPECT_TRUE(desk_1_mini_view->close_desk_button()->GetVisible());
  EXPECT_FALSE(desk_2_mini_view->close_desk_button()->GetVisible());
  EXPECT_FALSE(desk_3_mini_view->close_desk_button()->GetVisible());
  LongGestureTapOnView(desk_2_mini_view, event_generator);
  EXPECT_FALSE(desk_1_mini_view->close_desk_button()->GetVisible());
  EXPECT_TRUE(desk_2_mini_view->close_desk_button()->GetVisible());
  EXPECT_FALSE(desk_3_mini_view->close_desk_button()->GetVisible());

  // Tapping on the visible close button, closes the desk rather than switches
  // to that desk.
  GestureTapOnView(desk_2_mini_view->close_desk_button(), event_generator);
  ASSERT_EQ(2u, controller->desks().size());
  ASSERT_EQ(2u, desks_bar_view->mini_views().size());
  EXPECT_TRUE(overview_controller->InOverviewSession());

  // Tapping on the invisible close button should not result in closing that
  // desk; rather activating that desk.
  EXPECT_FALSE(desk_1_mini_view->close_desk_button()->GetVisible());
  GestureTapOnView(desk_1_mini_view->close_desk_button(), event_generator);
  ASSERT_EQ(2u, controller->desks().size());
  EXPECT_FALSE(overview_controller->InOverviewSession());
  EXPECT_TRUE(controller->desks()[0]->is_active());
}

class DesksWithSplitViewTest : public AshTestBase {
 public:
  DesksWithSplitViewTest() = default;
  ~DesksWithSplitViewTest() override = default;

  // AshTestBase:
  void SetUp() override {
    scoped_feature_list_.InitWithFeatures(
        /*enabled_features=*/{features::kVirtualDesks,
                              features::kDragToSnapInClamshellMode},
        /*disabled_features=*/{});

    AshTestBase::SetUp();
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;

  DISALLOW_COPY_AND_ASSIGN(DesksWithSplitViewTest);
};

TEST_F(DesksWithSplitViewTest, SuccessfulDragToDeskRemovesSplitViewIndicators) {
  auto* controller = DesksController::Get();
  controller->NewDesk();
  ASSERT_EQ(2u, controller->desks().size());
  auto window = CreateTestWindow(gfx::Rect(0, 0, 250, 100));
  wm::ActivateWindow(window.get());
  EXPECT_EQ(window.get(), wm::GetActiveWindow());

  auto* overview_controller = Shell::Get()->overview_controller();
  overview_controller->ToggleOverview();
  EXPECT_TRUE(overview_controller->InOverviewSession());
  auto* overview_grid = GetOverviewGridForRoot(Shell::GetPrimaryRootWindow());

  auto* overview_session = overview_controller->overview_session();
  auto* overview_item =
      overview_session->GetOverviewItemForWindow(window.get());
  ASSERT_TRUE(overview_item);
  const auto* desks_bar_view = overview_grid->GetDesksBarViewForTesting();
  ASSERT_TRUE(desks_bar_view);
  ASSERT_EQ(2u, desks_bar_view->mini_views().size());

  // Drag it to desk_2's mini_view. The overview grid should now show the
  // "no-windows" widget, and the window should move to desk_2.
  auto* desk_2_mini_view = desks_bar_view->mini_views()[1].get();
  DragItemToPoint(overview_item,
                  desk_2_mini_view->GetBoundsInScreen().CenterPoint(),
                  GetEventGenerator(), /*drop=*/false);
  // Validate that before dropping, the SplitView indicators and the drop target
  // widget are created.
  EXPECT_TRUE(overview_grid->drop_target_widget());
  EXPECT_EQ(IndicatorState::kDragArea,
            overview_session->split_view_drag_indicators()
                ->current_indicator_state());
  // Now drop the window, and validate the indicators and the drop target were
  // removed.
  GetEventGenerator()->ReleaseLeftButton();
  EXPECT_TRUE(overview_controller->InOverviewSession());
  EXPECT_TRUE(overview_grid->empty());
  EXPECT_FALSE(DoesActiveDeskContainWindow(window.get()));
  EXPECT_TRUE(overview_session->no_windows_widget_for_testing());
  EXPECT_FALSE(overview_grid->drop_target_widget());
  EXPECT_EQ(IndicatorState::kNone,
            overview_session->split_view_drag_indicators()
                ->current_indicator_state());
}

namespace {

constexpr char kUser1Email[] = "user1@desks";
constexpr char kUser2Email[] = "user2@desks";

}  // namespace

class DesksMultiUserTest : public NoSessionAshTestBase,
                           public MultiUserWindowManagerDelegate {
 public:
  DesksMultiUserTest() = default;
  ~DesksMultiUserTest() override = default;

  MultiUserWindowManager* multi_user_window_manager() {
    return multi_user_window_manager_.get();
  }

  // AshTestBase:
  void SetUp() override {
    scoped_feature_list_.InitAndEnableFeature(features::kVirtualDesks);

    NoSessionAshTestBase::SetUp();
    TestSessionControllerClient* session_controller =
        GetSessionControllerClient();
    session_controller->Reset();
    session_controller->AddUserSession(kUser1Email);
    session_controller->AddUserSession(kUser2Email);

    // Simulate user 1 login.
    SwitchActiveUser(GetUser1AccountId());
    multi_user_window_manager_ =
        MultiUserWindowManager::Create(this, GetUser1AccountId());
    MultiUserWindowManagerImpl::Get()->SetAnimationSpeedForTest(
        MultiUserWindowManagerImpl::ANIMATION_SPEED_DISABLED);
  }

  void TearDown() override {
    multi_user_window_manager_.reset();
    NoSessionAshTestBase::TearDown();
  }

  // MultiUserWindowManagerDelegate:
  void OnWindowOwnerEntryChanged(aura::Window* window,
                                 const AccountId& account_id,
                                 bool was_minimized,
                                 bool teleported) override {}
  void OnTransitionUserShelfToNewAccount() override {}

  AccountId GetUser1AccountId() const {
    return AccountId::FromUserEmail(kUser1Email);
  }

  AccountId GetUser2AccountId() const {
    return AccountId::FromUserEmail(kUser2Email);
  }

  void SwitchActiveUser(const AccountId& account_id) {
    GetSessionControllerClient()->SwitchActiveUser(account_id);
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
  std::unique_ptr<MultiUserWindowManager> multi_user_window_manager_;

  DISALLOW_COPY_AND_ASSIGN(DesksMultiUserTest);
};

TEST_F(DesksMultiUserTest, SwitchUsersBackAndForth) {
  // Create two desks with two windows on each, one window that belongs to the
  // first user, and the other belongs to the second.
  auto* controller = DesksController::Get();
  controller->NewDesk();
  ASSERT_EQ(2u, controller->desks().size());
  Desk* desk_1 = controller->desks()[0].get();
  Desk* desk_2 = controller->desks()[1].get();
  auto win0 = CreateTestWindow(gfx::Rect(0, 0, 250, 100));
  multi_user_window_manager()->SetWindowOwner(win0.get(), GetUser1AccountId());
  EXPECT_TRUE(win0->IsVisible());
  ActivateDesk(desk_2);
  auto win1 = CreateTestWindow(gfx::Rect(50, 50, 200, 200));
  multi_user_window_manager()->SetWindowOwner(win1.get(), GetUser1AccountId());
  EXPECT_FALSE(win0->IsVisible());
  EXPECT_TRUE(win1->IsVisible());

  // Switch to user_2 and expect no windows from user_1 is visible regardless of
  // the desk.
  SwitchActiveUser(GetUser2AccountId());
  EXPECT_FALSE(win0->IsVisible());
  EXPECT_FALSE(win1->IsVisible());

  auto win2 = CreateTestWindow(gfx::Rect(0, 0, 250, 200));
  multi_user_window_manager()->SetWindowOwner(win2.get(), GetUser2AccountId());
  EXPECT_TRUE(win2->IsVisible());
  ActivateDesk(desk_1);
  auto win3 = CreateTestWindow(gfx::Rect(0, 0, 250, 200));
  multi_user_window_manager()->SetWindowOwner(win3.get(), GetUser2AccountId());
  EXPECT_FALSE(win0->IsVisible());
  EXPECT_FALSE(win1->IsVisible());
  EXPECT_FALSE(win2->IsVisible());
  EXPECT_TRUE(win3->IsVisible());

  // Similarly when switching back to user_1.
  SwitchActiveUser(GetUser1AccountId());
  EXPECT_TRUE(win0->IsVisible());
  EXPECT_FALSE(win1->IsVisible());
  EXPECT_FALSE(win2->IsVisible());
  EXPECT_FALSE(win3->IsVisible());
  ActivateDesk(desk_2);
  EXPECT_FALSE(win0->IsVisible());
  EXPECT_TRUE(win1->IsVisible());
  EXPECT_FALSE(win2->IsVisible());
  EXPECT_FALSE(win3->IsVisible());
}

// TODO(afakhry): Add more tests:
// - Always on top windows are not tracked by any desk.
// - Reusing containers when desks are removed and created.

}  // namespace

}  // namespace ash
