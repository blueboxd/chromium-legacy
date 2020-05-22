// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/quick_answers/ui/quick_answers_view.h"

#include "ash/quick_answers/quick_answers_controller_impl.h"
#include "ash/test/ash_test_base.h"

namespace ash {

namespace {

constexpr int kMarginDip = 10;
constexpr int kSmallTop = 30;
constexpr gfx::Rect kDefaultAnchorBoundsInScreen =
    gfx::Rect(gfx::Point(500, 250), gfx::Size(80, 140));

}  // namespace

class QuickAnswersViewsTest : public AshTestBase {
 protected:
  QuickAnswersViewsTest() = default;
  QuickAnswersViewsTest(const QuickAnswersViewsTest&) = delete;
  QuickAnswersViewsTest& operator=(const QuickAnswersViewsTest&) = delete;
  ~QuickAnswersViewsTest() override = default;

  // AshTestBase:
  void SetUp() override {
    AshTestBase::SetUp();

    dummy_anchor_bounds_ = kDefaultAnchorBoundsInScreen;
    controller_ = std::make_unique<QuickAnswersControllerImpl>();
    CreateQuickAnswersView(dummy_anchor_bounds_, "dummy_title");
  }

  void TearDown() override {
    quick_answers_view_.reset();
    controller_.reset();

    AshTestBase::TearDown();
  }

  // Currently instantiated QuickAnswersView instance.
  QuickAnswersView* view() { return quick_answers_view_.get(); }

  // Needed to poll the current bounds of the mock anchor.
  const gfx::Rect& dummy_anchor_bounds() { return dummy_anchor_bounds_; }

  // Create a QuickAnswersView instance with custom anchor-bounds and
  // title-text.
  void CreateQuickAnswersView(const gfx::Rect anchor_bounds,
                              const char* title) {
    // Reset existing view if any.
    quick_answers_view_.reset();

    dummy_anchor_bounds_ = anchor_bounds;
    quick_answers_view_ = std::make_unique<QuickAnswersView>(
        dummy_anchor_bounds_, title,
        controller_->quick_answers_ui_controller());
  }

 private:
  std::unique_ptr<QuickAnswersView> quick_answers_view_;
  std::unique_ptr<QuickAnswersControllerImpl> controller_;
  gfx::Rect dummy_anchor_bounds_;
};

TEST_F(QuickAnswersViewsTest, DefaultLayoutAroundAnchor) {
  gfx::Rect view_bounds = view()->GetBoundsInScreen();
  gfx::Rect anchor_bounds = dummy_anchor_bounds();

  // Vertically aligned with anchor.
  EXPECT_EQ(view_bounds.x(), anchor_bounds.x());
  EXPECT_EQ(view_bounds.right(), anchor_bounds.right());

  // View is positioned above the anchor.
  EXPECT_EQ(view_bounds.bottom() + kMarginDip, anchor_bounds.y());
}

TEST_F(QuickAnswersViewsTest, PositionedBelowAnchorIfLessSpaceAbove) {
  gfx::Rect anchor_bounds = dummy_anchor_bounds();
  // Update anchor-bounds' position so that it does not leave enough vertical
  // space above it to show the QuickAnswersView.
  anchor_bounds.set_y(kSmallTop);

  CreateQuickAnswersView(anchor_bounds, "dummy_title");
  gfx::Rect view_bounds = view()->GetBoundsInScreen();

  // Anchor is positioned above the view.
  EXPECT_EQ(anchor_bounds.bottom() + kMarginDip, view_bounds.y());
}

}  // namespace ash
