// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/wm/overview/overview_window_drag_controller.h"

#include "ash/public/cpp/ash_features.h"
#include "ash/shell.h"
#include "ash/test/ash_test_base.h"
#include "ash/wm/mru_window_tracker.h"
#include "ash/wm/overview/overview_controller.h"
#include "ash/wm/overview/overview_grid.h"
#include "ash/wm/overview/overview_item.h"
#include "ash/wm/overview/overview_session.h"
#include "ash/wm/window_util.h"
#include "base/stl_util.h"
#include "base/test/scoped_feature_list.h"
#include "ui/events/test/event_generator.h"

namespace ash {

namespace {

// Drags the item by |x| and |y| and does not drop it.
void StartDraggingItemBy(OverviewItem* item,
                         int x,
                         int y,
                         ui::test::EventGenerator* event_generator) {
  const gfx::Point item_center =
      gfx::ToRoundedPoint(item->target_bounds().CenterPoint());
  event_generator->MoveMouseTo(item_center);
  event_generator->PressLeftButton();
  event_generator->MoveMouseBy(x, y);
}

// Waits for a window to be destroyed.
class WindowCloseWaiter : public aura::WindowObserver {
 public:
  WindowCloseWaiter(aura::Window* window) : window_(window) {
    DCHECK(window_);
    window_->AddObserver(this);
  }

  ~WindowCloseWaiter() override {
    if (window_)
      window_->RemoveObserver(this);
  }

  void Wait() {
    // Did window close already?
    if (!window_)
      return;

    run_loop_.Run();
  }

  // aura::WindowObserver:
  void OnWindowDestroyed(aura::Window* window) override {
    window_ = nullptr;
    run_loop_.Quit();
  }

 private:
  aura::Window* window_;
  base::RunLoop run_loop_;

  DISALLOW_COPY_AND_ASSIGN(WindowCloseWaiter);
};

}  // namespace

// Tests the behavior of window dragging in overview when both VirtualDesks and
// Clamshell SplitView features are both disabled.
class NoDesksNoSplitViewTest : public AshTestBase {
 public:
  NoDesksNoSplitViewTest() = default;
  ~NoDesksNoSplitViewTest() override = default;

  // AshTestBase:
  void SetUp() override {
    scoped_feature_list_.InitWithFeatures(
        /*enabled_features=*/{},
        /*disabled_features=*/{features::kVirtualDesks,
                               features::kDragToSnapInClamshellMode});

    AshTestBase::SetUp();
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;

  DISALLOW_COPY_AND_ASSIGN(NoDesksNoSplitViewTest);
};

TEST_F(NoDesksNoSplitViewTest, NormalDragIsNotPossible) {
  auto window = CreateTestWindow(gfx::Rect(0, 0, 250, 100));
  wm::ActivateWindow(window.get());
  EXPECT_EQ(window.get(), wm::GetActiveWindow());
  auto* overview_controller = Shell::Get()->overview_controller();
  overview_controller->StartOverview();
  EXPECT_TRUE(overview_controller->InOverviewSession());
  auto* overview_session = overview_controller->overview_session();
  auto* overview_item =
      overview_session->GetOverviewItemForWindow(window.get());
  ASSERT_TRUE(overview_item);

  auto* event_generator = GetEventGenerator();
  // Drag the item by an enough amount in X that would normally trigger the
  // normal drag mode.
  StartDraggingItemBy(overview_item, 50, 0, event_generator);
  OverviewWindowDragController* drag_controller =
      overview_session->window_drag_controller();
  EXPECT_EQ(OverviewWindowDragController::DragBehavior::kUndefined,
            drag_controller->current_drag_behavior());

  // Drop the window, and expect that overview mode exits and the window is
  // activated.
  event_generator->ReleaseLeftButton();
  EXPECT_FALSE(overview_controller->InOverviewSession());
  EXPECT_EQ(window.get(), wm::GetActiveWindow());
}

TEST_F(NoDesksNoSplitViewTest, CanDoDragToClose) {
  auto window = CreateTestWindow(gfx::Rect(0, 0, 250, 100));
  wm::ActivateWindow(window.get());
  EXPECT_EQ(window.get(), wm::GetActiveWindow());
  auto* overview_controller = Shell::Get()->overview_controller();
  overview_controller->StartOverview();
  EXPECT_TRUE(overview_controller->InOverviewSession());
  auto* overview_session = overview_controller->overview_session();
  auto* overview_item =
      overview_session->GetOverviewItemForWindow(window.get());
  ASSERT_TRUE(overview_item);

  auto* event_generator = GetEventGenerator();
  // Dragging with a bigger Y-component than X should trigger the drag-to-close
  // mode.
  StartDraggingItemBy(overview_item, 30, 50, event_generator);
  OverviewWindowDragController* drag_controller =
      overview_session->window_drag_controller();
  EXPECT_EQ(OverviewWindowDragController::DragBehavior::kDragToClose,
            drag_controller->current_drag_behavior());

  // Continue dragging vertically and drop. Expect that overview exists since
  // it's the only window on the grid.
  event_generator->MoveMouseBy(0, 200);
  // release() the window as it will be closed and destroyed when we drop it.
  aura::Window* window_ptr = window.release();
  WindowCloseWaiter waiter{window_ptr};
  event_generator->ReleaseLeftButton();
  waiter.Wait();
  EXPECT_FALSE(overview_controller->InOverviewSession());
  EXPECT_FALSE(base::Contains(
      Shell::Get()->mru_window_tracker()->BuildMruWindowList(kAllDesks),
      window_ptr));
}

}  // namespace ash
