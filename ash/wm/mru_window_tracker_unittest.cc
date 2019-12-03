// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/wm/mru_window_tracker.h"

#include "ash/public/cpp/shell_window_ids.h"
#include "ash/shell.h"
#include "ash/test/ash_test_base.h"
#include "ash/wm/window_state.h"
#include "ash/wm/window_util.h"
#include "ui/aura/client/aura_constants.h"
#include "ui/base/hit_test.h"
#include "ui/base/ui_base_types.h"
#include "ui/views/widget/widget_delegate.h"

namespace ash {

class MruWindowTrackerTest : public AshTestBase {
 public:
  MruWindowTrackerTest() = default;
  ~MruWindowTrackerTest() override = default;

  std::unique_ptr<aura::Window> CreateTestWindow() {
    return AshTestBase::CreateTestWindow(gfx::Rect(0, 0, 400, 400));
  }

  MruWindowTracker* mru_window_tracker() {
    return Shell::Get()->mru_window_tracker();
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(MruWindowTrackerTest);
};

// Basic test that the activation order is tracked.
TEST_F(MruWindowTrackerTest, Basic) {
  std::unique_ptr<aura::Window> w1(CreateTestWindow());
  std::unique_ptr<aura::Window> w2(CreateTestWindow());
  std::unique_ptr<aura::Window> w3(CreateTestWindow());
  wm::ActivateWindow(w3.get());
  wm::ActivateWindow(w2.get());
  wm::ActivateWindow(w1.get());

  MruWindowTracker::WindowList window_list =
      mru_window_tracker()->BuildMruWindowList(kActiveDesk);
  ASSERT_EQ(3u, window_list.size());
  EXPECT_EQ(w1.get(), window_list[0]);
  EXPECT_EQ(w2.get(), window_list[1]);
  EXPECT_EQ(w3.get(), window_list[2]);
}

// Tests that windows being dragged are only in the WindowList once.
TEST_F(MruWindowTrackerTest, DraggedWindowsInListOnlyOnce) {
  std::unique_ptr<aura::Window> w1(CreateTestWindow());
  wm::ActivateWindow(w1.get());

  // Start dragging the window.
  WindowState::Get(w1.get())->CreateDragDetails(gfx::Point(), HTRIGHT,
                                                ::wm::WINDOW_MOVE_SOURCE_TOUCH);

  // The dragged window should only be in the list once.
  MruWindowTracker::WindowList window_list =
      mru_window_tracker()->BuildWindowListIgnoreModal(kActiveDesk);
  EXPECT_EQ(1, std::count(window_list.begin(), window_list.end(), w1.get()));
}

class MruWindowTrackerOrderTest : public MruWindowTrackerTest,
                                  public ::testing::WithParamInterface<bool> {
 public:
  MruWindowTrackerOrderTest() {}
  ~MruWindowTrackerOrderTest() override = default;

  MruWindowTracker::WindowList BuildMruWindowList() const {
    return GetParam()
               ? Shell::Get()->mru_window_tracker()->BuildWindowListIgnoreModal(
                     kActiveDesk)
               : Shell::Get()->mru_window_tracker()->BuildMruWindowList(
                     kActiveDesk);
  }
};

namespace {

class TestDelegate : public views::WidgetDelegateView {
 public:
  TestDelegate() = default;
  ~TestDelegate() override = default;

  // views::WidgetDelegateView:
  ui::ModalType GetModalType() const override { return ui::MODAL_TYPE_SYSTEM; }
};

}  // namespace

// Test that minimized windows are not treated specially.
TEST_P(MruWindowTrackerOrderTest, MinimizedWindowsAreLru) {
  std::unique_ptr<aura::Window> w1(CreateTestWindow());
  std::unique_ptr<aura::Window> w2(CreateTestWindow());
  std::unique_ptr<aura::Window> w3(CreateTestWindow());

  // Make w3 always on top.
  w3->SetProperty(aura::client::kZOrderingKey,
                  ui::ZOrderLevel::kFloatingWindow);
  // They're in different container.
  EXPECT_NE(w3->parent(), w1->parent());

  std::unique_ptr<aura::Window> w4(CreateTestWindow());
  std::unique_ptr<aura::Window> w5(CreateTestWindow());
  std::unique_ptr<aura::Window> w6(CreateTestWindow());

  wm::ActivateWindow(w6.get());
  wm::ActivateWindow(w5.get());
  wm::ActivateWindow(w4.get());
  wm::ActivateWindow(w3.get());
  wm::ActivateWindow(w2.get());
  wm::ActivateWindow(w1.get());

  WindowState::Get(w1.get())->Minimize();
  WindowState::Get(w4.get())->Minimize();
  WindowState::Get(w5.get())->Minimize();

  // By minimizing the first window, we activate w2 which will move it to the
  // front of the MRU queue.
  EXPECT_TRUE(wm::IsActiveWindow(w2.get()));

  MruWindowTracker::WindowList window_list = BuildMruWindowList();
  EXPECT_EQ(w2.get(), window_list[0]);
  EXPECT_EQ(w1.get(), window_list[1]);
  EXPECT_EQ(w3.get(), window_list[2]);
  EXPECT_EQ(w4.get(), window_list[3]);
  EXPECT_EQ(w5.get(), window_list[4]);
  EXPECT_EQ(w6.get(), window_list[5]);

  std::unique_ptr<views::Widget> modal =
      CreateTestWidget(new TestDelegate(), kShellWindowId_Invalid);
  EXPECT_EQ(modal.get()->GetNativeView()->parent()->id(),
            kShellWindowId_SystemModalContainer);

  window_list = BuildMruWindowList();
  auto iter = window_list.begin();
  if (GetParam()) {
    EXPECT_EQ(w2.get(), *iter++);
    EXPECT_EQ(w1.get(), *iter++);
    EXPECT_EQ(w3.get(), *iter++);
    EXPECT_EQ(w4.get(), *iter++);
    EXPECT_EQ(w5.get(), *iter++);
    EXPECT_EQ(w6.get(), *iter++);
  }
  EXPECT_EQ(iter, window_list.end());
}

INSTANTIATE_TEST_SUITE_P(MruWindowTrackerOrder,
                         MruWindowTrackerOrderTest,
                         /*use ignore modal=*/::testing::Bool());

}  // namespace ash
