// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/wm/gestures/overview_gesture_handler.h"

#include "ash/root_window_controller.h"
#include "ash/shell.h"
#include "ash/test/ash_test_base.h"
#include "ash/wm/overview/overview_controller.h"
#include "ash/wm/overview/overview_highlight_controller.h"
#include "ash/wm/overview/overview_item.h"
#include "ash/wm/window_util.h"
#include "ui/aura/test/test_window_delegate.h"
#include "ui/aura/test/test_windows.h"
#include "ui/aura/window.h"
#include "ui/aura/window_event_dispatcher.h"
#include "ui/events/test/event_generator.h"
#include "ui/views/widget/widget.h"

namespace ash {

class OverviewGestureHandlerTest : public AshTestBase {
 public:
  OverviewGestureHandlerTest() = default;
  ~OverviewGestureHandlerTest() override = default;

  aura::Window* CreateWindow(const gfx::Rect& bounds) {
    return CreateTestWindowInShellWithDelegate(&delegate_, -1, bounds);
  }

  bool InOverviewSession() {
    return Shell::Get()->overview_controller()->InOverviewSession();
  }

  const aura::Window* GetHighlightedWindow() {
    auto* controller = Shell::Get()->overview_controller();
    if (!controller->InOverviewSession())
      return nullptr;

    auto* overview_session = controller->overview_session();
    OverviewItem* item =
        overview_session->highlight_controller()->GetHighlightedItem();
    if (!item)
      return nullptr;
    return item->GetWindow();
  }

  float vertical_threshold_pixels() const {
    return OverviewGestureHandler::vertical_threshold_pixels_;
  }

  float horizontal_threshold_pixels() const {
    return OverviewGestureHandler::horizontal_threshold_pixels_;
  }

 private:
  aura::test::TestWindowDelegate delegate_;

  DISALLOW_COPY_AND_ASSIGN(OverviewGestureHandlerTest);
};

// Tests a three fingers upwards scroll gesture to enter and a scroll down to
// exit overview.
TEST_F(OverviewGestureHandlerTest, VerticalScrolls) {
  gfx::Rect bounds(0, 0, 400, 400);
  aura::Window* root_window = Shell::GetPrimaryRootWindow();
  std::unique_ptr<aura::Window> window1(CreateWindow(bounds));
  std::unique_ptr<aura::Window> window2(CreateWindow(bounds));
  ui::test::EventGenerator generator(root_window, root_window);
  const float long_scroll = 2 * vertical_threshold_pixels();
  generator.ScrollSequence(gfx::Point(), base::TimeDelta::FromMilliseconds(5),
                           0, -long_scroll, 100, 3);
  EXPECT_TRUE(InOverviewSession());

  // Swiping up again does nothing.
  generator.ScrollSequence(gfx::Point(), base::TimeDelta::FromMilliseconds(5),
                           0, -long_scroll, 100, 3);
  EXPECT_TRUE(InOverviewSession());

  // Swiping down exits.
  generator.ScrollSequence(gfx::Point(), base::TimeDelta::FromMilliseconds(5),
                           0, long_scroll, 100, 3);
  EXPECT_FALSE(InOverviewSession());

  // Swiping down again does nothing.
  generator.ScrollSequence(gfx::Point(), base::TimeDelta::FromMilliseconds(5),
                           0, long_scroll, 100, 3);
  EXPECT_FALSE(InOverviewSession());
}

// Tests three finger horizontal scroll gesture to move selection left or right.
TEST_F(OverviewGestureHandlerTest, HorizontalScrollInOverview) {
  gfx::Rect bounds(0, 0, 400, 400);
  aura::Window* root_window = Shell::GetPrimaryRootWindow();
  std::unique_ptr<aura::Window> window1(CreateWindow(bounds));
  std::unique_ptr<aura::Window> window2(CreateWindow(bounds));
  std::unique_ptr<aura::Window> window3(CreateWindow(bounds));
  std::unique_ptr<aura::Window> window4(CreateWindow(bounds));
  std::unique_ptr<aura::Window> window5(CreateWindow(bounds));
  ui::test::EventGenerator generator(root_window, root_window);
  const float vertical_scroll = 2 * vertical_threshold_pixels();
  const float horizontal_scroll = horizontal_threshold_pixels();
  // Enter overview mode as if using an accelerator.
  // Entering overview mode with an upwards three-finger scroll gesture would
  // have the same result (allow selection using horizontal scroll).
  Shell::Get()->overview_controller()->StartOverview();
  EXPECT_TRUE(InOverviewSession());

  // Scrolls until a window is highlight, ignoring any desks items (if any).
  auto scroll_until_window_highlighted = [&generator, this](float x_offset,
                                                            float y_offset) {
    do {
      generator.ScrollSequence(gfx::Point(),
                               base::TimeDelta::FromMilliseconds(5), x_offset,
                               y_offset,
                               /*steps=*/100, /*num_fingers=*/3);
    } while (!GetHighlightedWindow());
  };

  // Select the first window first.
  scroll_until_window_highlighted(horizontal_scroll, 0);

  // Long scroll right moves selection to the fourth window.
  scroll_until_window_highlighted(horizontal_scroll * 3, 0);
  EXPECT_TRUE(InOverviewSession());

  // Short scroll left (3 fingers) moves selection to the third window.
  scroll_until_window_highlighted(-horizontal_scroll, 0);
  EXPECT_TRUE(InOverviewSession());

  // Short scroll left (3 fingers) moves selection to the second window.
  scroll_until_window_highlighted(-horizontal_scroll, 0);
  EXPECT_TRUE(InOverviewSession());

  // Swiping down exits and selects the currently-highlighted window.
  generator.ScrollSequence(gfx::Point(), base::TimeDelta::FromMilliseconds(5),
                           0, vertical_scroll, 100, 3);
  EXPECT_FALSE(InOverviewSession());

  // Second MRU window is selected (i.e. |window4|).
  EXPECT_EQ(window4.get(), wm::GetActiveWindow());
}

// Tests that a mostly horizontal three-finger scroll does not trigger overview.
TEST_F(OverviewGestureHandlerTest, HorizontalScrolls) {
  gfx::Rect bounds(0, 0, 400, 400);
  aura::Window* root_window = Shell::GetPrimaryRootWindow();
  std::unique_ptr<aura::Window> window1(CreateWindow(bounds));
  std::unique_ptr<aura::Window> window2(CreateWindow(bounds));
  ui::test::EventGenerator generator(root_window, root_window);
  const float long_scroll = 2 * vertical_threshold_pixels();
  generator.ScrollSequence(gfx::Point(), base::TimeDelta::FromMilliseconds(5),
                           long_scroll + 100, -long_scroll, 100, 3);
  EXPECT_FALSE(InOverviewSession());

  generator.ScrollSequence(gfx::Point(), base::TimeDelta::FromMilliseconds(5),
                           -long_scroll - 100, -long_scroll, 100, 3);
  EXPECT_FALSE(InOverviewSession());
}

// Tests a scroll up with three fingers without releasing followed by a scroll
// down by a lesser amount which should still be enough to exit.
TEST_F(OverviewGestureHandlerTest, ScrollUpDownWithoutReleasing) {
  gfx::Rect bounds(0, 0, 400, 400);
  aura::Window* root_window = Shell::GetPrimaryRootWindow();
  std::unique_ptr<aura::Window> window1(CreateWindow(bounds));
  std::unique_ptr<aura::Window> window2(CreateWindow(bounds));
  ui::test::EventGenerator generator(root_window, root_window);
  base::TimeTicks timestamp = base::TimeTicks::Now();
  gfx::Point start;
  int num_fingers = 3;
  base::TimeDelta step_delay(base::TimeDelta::FromMilliseconds(5));
  ui::ScrollEvent fling_cancel(ui::ET_SCROLL_FLING_CANCEL, start, timestamp, 0,
                               0, 0, 0, 0, num_fingers);
  generator.Dispatch(&fling_cancel);

  // Scroll up by 1000px.
  for (int i = 0; i < 100; ++i) {
    timestamp += step_delay;
    ui::ScrollEvent move(ui::ET_SCROLL, start, timestamp, 0, 0, -10, 0, -10,
                         num_fingers);
    generator.Dispatch(&move);
  }

  EXPECT_TRUE(InOverviewSession());

  // Without releasing scroll back down by 600px.
  for (int i = 0; i < 60; ++i) {
    timestamp += step_delay;
    ui::ScrollEvent move(ui::ET_SCROLL, start, timestamp, 0, 0, 10, 0, 10,
                         num_fingers);
    generator.Dispatch(&move);
  }

  EXPECT_FALSE(InOverviewSession());
  ui::ScrollEvent fling_start(ui::ET_SCROLL_FLING_START, start, timestamp, 0, 0,
                              10, 0, 10, num_fingers);
  generator.Dispatch(&fling_start);
}

}  // namespace ash
