// Copyright (c) 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/shelf/scrollable_shelf_view.h"

#include "ash/drag_drop/drag_image_view.h"
#include "ash/public/cpp/shelf_config.h"
#include "ash/root_window_controller.h"
#include "ash/shelf/shelf_test_util.h"
#include "ash/shelf/shelf_tooltip_manager.h"
#include "ash/shelf/shelf_view_test_api.h"
#include "ash/shelf/shelf_widget.h"
#include "ash/shell.h"
#include "ash/test/ash_test_base.h"
#include "ash/wm/tablet_mode/tablet_mode_controller.h"
#include "base/test/scoped_feature_list.h"
#include "chromeos/constants/chromeos_features.h"
#include "ui/display/manager/display_manager.h"

namespace ash {

class PageFlipWaiter : public ScrollableShelfView::TestObserver {
 public:
  explicit PageFlipWaiter(ScrollableShelfView* scrollable_shelf_view)
      : scrollable_shelf_view_(scrollable_shelf_view) {
    scrollable_shelf_view->SetTestObserver(this);
  }
  ~PageFlipWaiter() override {
    scrollable_shelf_view_->SetTestObserver(nullptr);
  }

  void Wait() {
    run_loop_ = std::make_unique<base::RunLoop>();
    run_loop_->Run();
  }

 private:
  void OnPageFlipTimerFired() override {
    DCHECK(run_loop_.get());
    run_loop_->Quit();
  }

  ScrollableShelfView* scrollable_shelf_view_ = nullptr;
  std::unique_ptr<base::RunLoop> run_loop_;
};

class TestShelfItemDelegate : public ShelfItemDelegate {
 public:
  explicit TestShelfItemDelegate(const ShelfID& shelf_id)
      : ShelfItemDelegate(shelf_id) {}

  // ShelfItemDelegate:
  void ItemSelected(std::unique_ptr<ui::Event> event,
                    int64_t display_id,
                    ShelfLaunchSource source,
                    ItemSelectedCallback callback) override {
    std::move(callback).Run(SHELF_ACTION_WINDOW_ACTIVATED, {});
  }
  void ExecuteCommand(bool from_context_menu,
                      int64_t command_id,
                      int32_t event_flags,
                      int64_t display_id) override {}
  void Close() override {}
};

class ScrollableShelfViewTest : public AshTestBase {
 public:
  ScrollableShelfViewTest() = default;
  ~ScrollableShelfViewTest() override = default;

  void SetUp() override {
    scoped_feature_list_.InitWithFeatures(
        {chromeos::features::kShelfScrollable}, {});

    AshTestBase::SetUp();
    scrollable_shelf_view_ = GetPrimaryShelf()
                                 ->shelf_widget()
                                 ->hotseat_widget()
                                 ->scrollable_shelf_view();
    shelf_view_ = scrollable_shelf_view_->shelf_view();
    test_api_ = std::make_unique<ShelfViewTestAPI>(
        scrollable_shelf_view_->shelf_view());
    test_api_->SetAnimationDuration(base::TimeDelta::FromMilliseconds(1));
  }

  void TearDown() override {
    scoped_feature_list_.Reset();
    AshTestBase::TearDown();
  }

 protected:
  void PopulateAppShortcut(int number) {
    for (int i = 0; i < number; i++)
      AddAppShortcut();
  }

  ShelfID AddAppShortcut(ShelfItemType item_type = TYPE_PINNED_APP) {
    ShelfItem item =
        ShelfTestUtil::AddAppShortcut(base::NumberToString(id_++), item_type);

    // Wait for shelf view's bounds animation to end. Otherwise the scrollable
    // shelf's bounds are not updated yet.
    test_api_->RunMessageLoopUntilAnimationsDone();

    return item.id;
  }

  void AddAppShortcutsUntilOverflow() {
    while (scrollable_shelf_view_->layout_strategy_for_test() ==
           ScrollableShelfView::kNotShowArrowButtons)
      AddAppShortcut();
  }

  void AddAppShortcutsUntilRightArrowIsShown() {
    ASSERT_FALSE(scrollable_shelf_view_->right_arrow()->GetVisible());

    while (!scrollable_shelf_view_->right_arrow()->GetVisible())
      AddAppShortcut();
  }

  void CheckFirstAndLastTappableIconsBounds() {
    views::ViewModel* view_model = shelf_view_->view_model();

    gfx::Rect visible_space_in_screen = scrollable_shelf_view_->visible_space();
    views::View::ConvertRectToScreen(scrollable_shelf_view_,
                                     &visible_space_in_screen);

    views::View* last_tappable_icon =
        view_model->view_at(scrollable_shelf_view_->last_tappable_app_index());
    const gfx::Rect last_tappable_icon_bounds =
        last_tappable_icon->GetBoundsInScreen();

    // Expects that the last tappable icon is fully shown.
    EXPECT_TRUE(visible_space_in_screen.Contains(last_tappable_icon_bounds));

    views::View* first_tappable_icon =
        view_model->view_at(scrollable_shelf_view_->first_tappable_app_index());
    const gfx::Rect first_tappable_icon_bounds =
        first_tappable_icon->GetBoundsInScreen();

    // Expects that the first tappable icon is fully shown.
    EXPECT_TRUE(visible_space_in_screen.Contains(first_tappable_icon_bounds));
  }

  base::test::ScopedFeatureList scoped_feature_list_;
  ScrollableShelfView* scrollable_shelf_view_ = nullptr;
  ShelfView* shelf_view_ = nullptr;
  std::unique_ptr<ShelfViewTestAPI> test_api_;
  int id_ = 0;
};

// Verifies that the display rotation from the short side to the long side
// should not break the scrollable shelf's UI
// behavior(https://crbug.com/1000764).
TEST_F(ScrollableShelfViewTest, CorrectUIAfterDisplayRotationShortToLong) {
  // Changes the display setting in order that the display's height is greater
  // than the width.
  UpdateDisplay("600x800");

  display::Display display = GetPrimaryDisplay();

  // Adds enough app icons so that after display rotation the scrollable
  // shelf is still in overflow mode.
  const int num = display.bounds().height() / ShelfConfig::Get()->button_size();
  for (int i = 0; i < num; i++)
    AddAppShortcut();

  // Because the display's height is greater than the display's width,
  // the scrollable shelf is in overflow mode before display rotation.
  ASSERT_EQ(ScrollableShelfView::kShowRightArrowButton,
            scrollable_shelf_view_->layout_strategy_for_test());

  // Presses the right arrow until reaching the last page of shelf icons.
  const views::View* right_arrow = scrollable_shelf_view_->right_arrow();
  const gfx::Point center_point =
      right_arrow->GetBoundsInScreen().CenterPoint();
  while (right_arrow->GetVisible()) {
    GetEventGenerator()->MoveMouseTo(center_point);
    GetEventGenerator()->PressLeftButton();
    GetEventGenerator()->ReleaseLeftButton();
  }
  ASSERT_EQ(ScrollableShelfView::kShowLeftArrowButton,
            scrollable_shelf_view_->layout_strategy_for_test());

  // Rotates the display by 90 degrees.
  display::DisplayManager* display_manager = Shell::Get()->display_manager();
  display_manager->SetDisplayRotation(display.id(), display::Display::ROTATE_90,
                                      display::Display::RotationSource::ACTIVE);

  // After rotation, checks the following things:
  // (1) The scrollable shelf has the correct layout strategy.
  // (2) The last app icon has the correct bounds.
  // (3) The scrollable shelf does not need further adjustment.
  EXPECT_EQ(ScrollableShelfView::kShowLeftArrowButton,
            scrollable_shelf_view_->layout_strategy_for_test());
  views::ViewModel* view_model = shelf_view_->view_model();
  const views::View* last_visible_icon =
      view_model->view_at(scrollable_shelf_view_->last_tappable_app_index());
  const gfx::Rect icon_bounds = last_visible_icon->GetBoundsInScreen();
  gfx::Rect visible_space = scrollable_shelf_view_->visible_space();
  views::View::ConvertRectToScreen(scrollable_shelf_view_, &visible_space);
  EXPECT_EQ(icon_bounds.right() +
                ShelfConfig::Get()->scrollable_shelf_ripple_padding(),
            visible_space.right());
  EXPECT_FALSE(scrollable_shelf_view_->ShouldAdjustForTest());
}

// Verifies that the display rotation from the long side to the short side
// should not break the scrollable shelf's UI behavior
// (https://crbug.com/1000764).
TEST_F(ScrollableShelfViewTest, CorrectUIAfterDisplayRotationLongToShort) {
  // Changes the display setting in order that the display's width is greater
  // than the height.
  UpdateDisplay("600x300");

  display::Display display = GetPrimaryDisplay();
  AddAppShortcutsUntilOverflow();

  // Presses the right arrow until reaching the last page of shelf icons.
  const views::View* right_arrow = scrollable_shelf_view_->right_arrow();
  const gfx::Point center_point =
      right_arrow->GetBoundsInScreen().CenterPoint();
  while (right_arrow->GetVisible()) {
    GetEventGenerator()->MoveMouseTo(center_point);
    GetEventGenerator()->PressLeftButton();
    GetEventGenerator()->ReleaseLeftButton();
  }
  ASSERT_EQ(ScrollableShelfView::kShowLeftArrowButton,
            scrollable_shelf_view_->layout_strategy_for_test());

  // Rotates the display by 90 degrees. In order to reproduce the bug,
  // both arrow buttons should show after rotation.
  display::DisplayManager* display_manager = Shell::Get()->display_manager();
  display_manager->SetDisplayRotation(display.id(), display::Display::ROTATE_90,
                                      display::Display::RotationSource::ACTIVE);
  ASSERT_EQ(ScrollableShelfView::kShowButtons,
            scrollable_shelf_view_->layout_strategy_for_test());

  // Verifies that the scrollable shelf does not need further adjustment.
  EXPECT_FALSE(scrollable_shelf_view_->ShouldAdjustForTest());
}

// When hovering mouse on a shelf icon, the tooltip only shows for the visible
// icon (see https://crbug.com/997807).
TEST_F(ScrollableShelfViewTest, NotShowTooltipForHiddenIcons) {
  AddAppShortcutsUntilOverflow();

  ASSERT_EQ(ScrollableShelfView::kShowRightArrowButton,
            scrollable_shelf_view_->layout_strategy_for_test());
  scrollable_shelf_view_->first_tappable_app_index();

  views::ViewModel* view_model = shelf_view_->view_model();

  // Check the initial state of |tooltip_manager|.
  ShelfTooltipManager* tooltip_manager = test_api_->tooltip_manager();
  EXPECT_FALSE(tooltip_manager->IsVisible());

  // Verifies that tooltip should show for a visible shelf item.
  views::View* visible_icon =
      view_model->view_at(scrollable_shelf_view_->first_tappable_app_index());
  GetEventGenerator()->MoveMouseTo(
      visible_icon->GetBoundsInScreen().CenterPoint());
  tooltip_manager->ShowTooltip(visible_icon);
  EXPECT_TRUE(tooltip_manager->IsVisible());

  // Reset |tooltip_manager|.
  GetEventGenerator()->MoveMouseTo(gfx::Point());
  tooltip_manager->Close();
  EXPECT_FALSE(tooltip_manager->IsVisible());

  // Verifies that tooltip should not show for a hidden shelf item.
  views::View* hidden_icon = view_model->view_at(
      scrollable_shelf_view_->last_tappable_app_index() + 1);
  GetEventGenerator()->MoveMouseTo(
      hidden_icon->GetBoundsInScreen().CenterPoint());
  tooltip_manager->ShowTooltip(hidden_icon);
  EXPECT_FALSE(tooltip_manager->IsVisible());
}

// Test that tapping near the scroll arrow button triggers scrolling. (see
// https://crbug.com/1004998)
TEST_F(ScrollableShelfViewTest, ScrollAfterTappingNearScrollArrow) {
  AddAppShortcutsUntilOverflow();

  ASSERT_EQ(ScrollableShelfView::kShowRightArrowButton,
            scrollable_shelf_view_->layout_strategy_for_test());

  // Tap right arrow and check that the scrollable shelf now shows the left
  // arrow only. Then do the same for the left arrow.
  const gfx::Rect right_arrow =
      scrollable_shelf_view_->right_arrow()->GetBoundsInScreen();
  GetEventGenerator()->GestureTapAt(right_arrow.CenterPoint());
  ASSERT_EQ(ScrollableShelfView::kShowLeftArrowButton,
            scrollable_shelf_view_->layout_strategy_for_test());

  const gfx::Rect left_arrow =
      scrollable_shelf_view_->left_arrow()->GetBoundsInScreen();
  GetEventGenerator()->GestureTapAt(left_arrow.CenterPoint());
  ASSERT_EQ(ScrollableShelfView::kShowRightArrowButton,
            scrollable_shelf_view_->layout_strategy_for_test());

  // Recalculate the right arrow  bounds considering the padding for the tap
  // area.
  const int horizontalPadding = (32 - right_arrow.width()) / 2;
  const int verticalPadding =
      (ShelfConfig::Get()->button_size() - right_arrow.height()) / 2;

  // Tap near the right arrow and check that the scrollable shelf now shows the
  // left arrow only. Then do the same for the left arrow.
  GetEventGenerator()->GestureTapAt(
      gfx::Point(right_arrow.top_right().x() - horizontalPadding,
                 right_arrow.top_right().y() + verticalPadding));
  ASSERT_EQ(ScrollableShelfView::kShowLeftArrowButton,
            scrollable_shelf_view_->layout_strategy_for_test());

  GetEventGenerator()->GestureTapAt(
      gfx::Point(left_arrow.top_right().x(), left_arrow.top_right().y()));
  EXPECT_EQ(ScrollableShelfView::kShowRightArrowButton,
            scrollable_shelf_view_->layout_strategy_for_test());
}

// Verifies that in overflow mode, the app icons indexed by
// |first_tappable_app_index_| and |last_tappable_app_index_| are completely
// shown (https://crbug.com/1013811).
TEST_F(ScrollableShelfViewTest, VerifyTappableAppIndices) {
  AddAppShortcutsUntilOverflow();

  // Checks bounds when the layout strategy is kShowRightArrowButton.
  ASSERT_EQ(ScrollableShelfView::kShowRightArrowButton,
            scrollable_shelf_view_->layout_strategy_for_test());
  CheckFirstAndLastTappableIconsBounds();

  // Pins enough apps to Shelf to ensure that layout strategy will be
  // kShowButtons after pressing the right arrow button.
  const int view_size =
      scrollable_shelf_view_->shelf_view()->view_model()->view_size();
  PopulateAppShortcut(view_size + 1);
  GetEventGenerator()->GestureTapAt(
      scrollable_shelf_view_->right_arrow()->GetBoundsInScreen().CenterPoint());

  // Checks bounds when the layout strategy is kShowButtons.
  ASSERT_EQ(ScrollableShelfView::kShowButtons,
            scrollable_shelf_view_->layout_strategy_for_test());
  CheckFirstAndLastTappableIconsBounds();

  GetEventGenerator()->GestureTapAt(
      scrollable_shelf_view_->right_arrow()->GetBoundsInScreen().CenterPoint());

  // Checks bounds when the layout strategy is kShowLeftArrowButton.
  ASSERT_EQ(ScrollableShelfView::kShowLeftArrowButton,
            scrollable_shelf_view_->layout_strategy_for_test());
  CheckFirstAndLastTappableIconsBounds();
}

TEST_F(ScrollableShelfViewTest, ShowTooltipForArrowButtons) {
  AddAppShortcutsUntilOverflow();
  ASSERT_EQ(ScrollableShelfView::kShowRightArrowButton,
            scrollable_shelf_view_->layout_strategy_for_test());

  // Check the initial state of |tooltip_manager|.
  ShelfTooltipManager* tooltip_manager = test_api_->tooltip_manager();
  EXPECT_FALSE(tooltip_manager->IsVisible());

  // Verifies that tooltip should show for a visible shelf item.
  views::View* right_arrow = scrollable_shelf_view_->right_arrow();
  GetEventGenerator()->MoveMouseTo(
      right_arrow->GetBoundsInScreen().CenterPoint());
  tooltip_manager->ShowTooltip(right_arrow);
  EXPECT_TRUE(tooltip_manager->IsVisible());

  // Click right arrow button to scroll the shelf and show left arrow button.
  GetEventGenerator()->ClickLeftButton();
  ASSERT_EQ(ScrollableShelfView::kShowLeftArrowButton,
            scrollable_shelf_view_->layout_strategy_for_test());

  // Reset |tooltip_manager|.
  GetEventGenerator()->MoveMouseTo(gfx::Point());
  tooltip_manager->Close();
  EXPECT_FALSE(tooltip_manager->IsVisible());

  views::View* left_arrow = scrollable_shelf_view_->left_arrow();
  GetEventGenerator()->MoveMouseTo(
      left_arrow->GetBoundsInScreen().CenterPoint());
  tooltip_manager->ShowTooltip(left_arrow);
  EXPECT_TRUE(tooltip_manager->IsVisible());
}

// Verifies that dragging an app icon to a new shelf page works well. In
// addition, the dragged icon moves with mouse before mouse release (see
// https://crbug.com/1031367).
TEST_F(ScrollableShelfViewTest, DragIconToNewPage) {
  scrollable_shelf_view_->set_page_flip_time_threshold(
      base::TimeDelta::FromMilliseconds(10));

  AddAppShortcutsUntilOverflow();
  GetEventGenerator()->GestureTapAt(
      scrollable_shelf_view_->right_arrow()->GetBoundsInScreen().CenterPoint());
  ASSERT_EQ(ScrollableShelfView::kShowLeftArrowButton,
            scrollable_shelf_view_->layout_strategy_for_test());

  views::ViewModel* view_model = shelf_view_->view_model();
  views::View* dragged_view =
      view_model->view_at(scrollable_shelf_view_->last_tappable_app_index());
  const gfx::Point drag_start_point =
      dragged_view->GetBoundsInScreen().CenterPoint();

  // Ensures that the app icon is not dragged to the ideal bounds directly.
  // It helps to construct a more complex scenario that the animation
  // is created to move the dropped icon to the target place after drag release.
  const gfx::Point drag_end_point =
      scrollable_shelf_view_->left_arrow()->GetBoundsInScreen().origin() -
      gfx::Vector2d(10, 0);

  ASSERT_NE(0, view_model->GetIndexOfView(dragged_view));

  // Drag |dragged_view| from |drag_start_point| to |drag_end_point|. Wait
  // for enough time before releasing the mouse button.
  GetEventGenerator()->MoveMouseTo(drag_start_point);
  GetEventGenerator()->PressLeftButton();
  GetEventGenerator()->MoveMouseTo(drag_end_point);
  {
    PageFlipWaiter waiter(scrollable_shelf_view_);
    waiter.Wait();
  }

  // Expects that the drag icon moves with drag pointer before mouse release.
  const gfx::Rect intermediate_bounds =
      scrollable_shelf_view_->drag_icon_for_test()->GetBoundsInScreen();
  EXPECT_EQ(drag_end_point, intermediate_bounds.CenterPoint());

  GetEventGenerator()->ReleaseLeftButton();
  ASSERT_NE(intermediate_bounds.CenterPoint(),
            dragged_view->GetBoundsInScreen().CenterPoint());

  // Expects that the proxy icon is deleted after mouse release.
  EXPECT_EQ(nullptr, scrollable_shelf_view_->drag_icon_for_test());

  // Verifies that:
  // (1) Scrollable shelf view has the expected layout strategy.
  // (2) The dragged view has the correct view index.
  EXPECT_EQ(ScrollableShelfView::kShowRightArrowButton,
            scrollable_shelf_view_->layout_strategy_for_test());
  const int view_index = view_model->GetIndexOfView(dragged_view);
  EXPECT_GE(view_index, scrollable_shelf_view_->first_tappable_app_index());
  EXPECT_LE(view_index, scrollable_shelf_view_->last_tappable_app_index());
}

class HotseatScrollableShelfViewTest : public ScrollableShelfViewTest {
 public:
  HotseatScrollableShelfViewTest() = default;
  ~HotseatScrollableShelfViewTest() override = default;

  void SetUp() override {
    scoped_feature_list_.InitWithFeatures({chromeos::features::kShelfHotseat},
                                          {});
    ScrollableShelfViewTest::SetUp();
  }

  void TearDown() override {
    ScrollableShelfViewTest::TearDown();
    scoped_feature_list_.Reset();
  }

  bool HasRoundedCornersOnLongTapAtLocation(gfx::Point location) {
    GetEventGenerator()->MoveTouch(location);
    GetEventGenerator()->PressTouch();

    // The gfx::RoundedCornersF object is considered empty when all of the
    // corners are squared (no effective radius).
    bool has_rounded_corners = !(scrollable_shelf_view_->shelf_container_view()
                                     ->layer()
                                     ->rounded_corner_radii()
                                     .IsEmpty());
    GetEventGenerator()->ReleaseTouch();
    return has_rounded_corners;
  }

  base::test::ScopedFeatureList scoped_feature_list_;
};

// Verifies that after adding the second display, shelf icons showing on
// the primary display are also visible on the second display
// (https://crbug.com/1035596).
TEST_F(HotseatScrollableShelfViewTest, CheckTappableIndicesOnSecondDisplay) {
  constexpr int icon_number = 5;
  for (int i = 0; i < icon_number; i++)
    AddAppShortcut();

  // Adds the second display.
  UpdateDisplay("600x800,600x800");

  Shelf* secondary_shelf =
      Shell::GetRootWindowControllerWithDisplayId(GetSecondaryDisplay().id())
          ->shelf();
  ScrollableShelfView* secondary_scrollable_shelf_view =
      secondary_shelf->shelf_widget()
          ->hotseat_widget()
          ->scrollable_shelf_view();

  // Verifies that the all icons are visible on the secondary display.
  EXPECT_EQ(icon_number - 1,
            secondary_scrollable_shelf_view->last_tappable_app_index());
  EXPECT_EQ(0, secondary_scrollable_shelf_view->first_tappable_app_index());
}

// Verifies that the scrollable shelf in oveflow mode has the correct layout
// after switching to tablet mode (https://crbug.com/1017979).
TEST_F(HotseatScrollableShelfViewTest, CorrectUIAfterSwitchingToTablet) {
  // Add enough app shortcuts to ensure that at least three pages of icons show.
  for (int i = 0; i < 25; i++)
    AddAppShortcut();
  ASSERT_EQ(ScrollableShelfView::kShowRightArrowButton,
            scrollable_shelf_view_->layout_strategy_for_test());
  GetEventGenerator()->GestureTapAt(
      scrollable_shelf_view_->right_arrow()->GetBoundsInScreen().CenterPoint());
  ASSERT_EQ(ScrollableShelfView::kShowButtons,
            scrollable_shelf_view_->layout_strategy_for_test());

  Shell::Get()->tablet_mode_controller()->SetEnabledForTest(true);
  base::RunLoop().RunUntilIdle();

  views::ViewModel* view_model = shelf_view_->view_model();
  views::View* first_tappable_view =
      view_model->view_at(scrollable_shelf_view_->first_tappable_app_index());

  // Verifies that the gap between the left arrow button and the first tappable
  // icon is expected.
  const gfx::Rect left_arrow_bounds =
      scrollable_shelf_view_->left_arrow()->GetBoundsInScreen();
  EXPECT_EQ(left_arrow_bounds.right() + 2,
            first_tappable_view->GetBoundsInScreen().x());
}

// Verifies that the scrollable shelf without overflow has the correct layout in
// tablet mode.
TEST_F(HotseatScrollableShelfViewTest, CorrectUIInTabletWithoutOverflow) {
  Shell::Get()->tablet_mode_controller()->SetEnabledForTest(true);

  for (int i = 0; i < 3; i++)
    AddAppShortcut();
  ASSERT_EQ(ScrollableShelfView::kNotShowArrowButtons,
            scrollable_shelf_view_->layout_strategy_for_test());

  gfx::Rect hotseat_background =
      scrollable_shelf_view_->GetHotseatBackgroundBounds();
  views::View::ConvertRectToScreen(scrollable_shelf_view_, &hotseat_background);

  views::ViewModel* view_model = shelf_view_->view_model();
  const gfx::Rect first_tappable_view_bounds =
      view_model->view_at(scrollable_shelf_view_->first_tappable_app_index())
          ->GetBoundsInScreen();
  const gfx::Rect last_tappable_view_bounds =
      view_model->view_at(scrollable_shelf_view_->last_tappable_app_index())
          ->GetBoundsInScreen();

  EXPECT_EQ(hotseat_background.x() + 4, first_tappable_view_bounds.x());
  EXPECT_EQ(hotseat_background.right() - 4, last_tappable_view_bounds.right());
}

// Verifies that the scrollable shelf without overflow has the correct layout in
// tablet mode.
TEST_F(HotseatScrollableShelfViewTest, CheckRoundedCornersSetForInkDrop) {
  Shell::Get()->tablet_mode_controller()->SetEnabledForTest(true);
  AddAppShortcutsUntilOverflow();
  ASSERT_EQ(ScrollableShelfView::kShowRightArrowButton,
            scrollable_shelf_view_->layout_strategy_for_test());
  ASSERT_TRUE(scrollable_shelf_view_->shelf_container_view()
                  ->layer()
                  ->rounded_corner_radii()
                  .IsEmpty());

  views::ViewModel* view_model = shelf_view_->view_model();
  gfx::Rect first_tappable_view_bounds =
      view_model->view_at(scrollable_shelf_view_->first_tappable_app_index())
          ->GetBoundsInScreen();
  gfx::Rect last_tappable_view_bounds =
      view_model->view_at(scrollable_shelf_view_->last_tappable_app_index())
          ->GetBoundsInScreen();

  // When the right arrow is showing, check rounded corners are set if the ink
  // drop is visible for the first visible app.
  EXPECT_TRUE(HasRoundedCornersOnLongTapAtLocation(
      first_tappable_view_bounds.CenterPoint()));
  // Tap outside the app and verify that rounded corners are not set if the ink
  // drop is hidden.
  GetEventGenerator()->GestureTapAt(first_tappable_view_bounds.bottom_center());
  EXPECT_TRUE(scrollable_shelf_view_->shelf_container_view()
                  ->layer()
                  ->rounded_corner_radii()
                  .IsEmpty());
  // When the right arrow is showing, check rounded corners are not set if the
  // ink drop is visible for the last visible app
  EXPECT_FALSE(HasRoundedCornersOnLongTapAtLocation(
      last_tappable_view_bounds.CenterPoint()));

  // Tap right arrow. Hotseat layout must now show left arrow.
  gfx::Rect right_arrow =
      scrollable_shelf_view_->right_arrow()->GetBoundsInScreen();
  GetEventGenerator()->GestureTapAt(right_arrow.CenterPoint());
  ASSERT_EQ(ScrollableShelfView::kShowLeftArrowButton,
            scrollable_shelf_view_->layout_strategy_for_test());

  // Recalculate first and last view bounds.
  first_tappable_view_bounds =
      view_model->view_at(scrollable_shelf_view_->first_tappable_app_index())
          ->GetBoundsInScreen();
  last_tappable_view_bounds =
      view_model->view_at(scrollable_shelf_view_->last_tappable_app_index())
          ->GetBoundsInScreen();

  // When the left arrow is showing, check rounded corners are set if the ink
  // drop is visible for the last visible app.
  EXPECT_TRUE(HasRoundedCornersOnLongTapAtLocation(
      last_tappable_view_bounds.CenterPoint()));
  // Tap outside the app and verify that rounded corners are not set if the ink
  // drop is hidden.
  GetEventGenerator()->GestureTapAt(last_tappable_view_bounds.bottom_center());
  EXPECT_TRUE(scrollable_shelf_view_->shelf_container_view()
                  ->layer()
                  ->rounded_corner_radii()
                  .IsEmpty());
  // When the left arrow is showing, check rounded corners are not set if the
  // ink drop is visible for the first visible app
  EXPECT_FALSE(HasRoundedCornersOnLongTapAtLocation(
      first_tappable_view_bounds.CenterPoint()));
}

// Verifies that doing a mousewheel scroll on the scrollable shelf does scroll
// forward.
TEST_F(ScrollableShelfViewTest, ScrollWithMouseWheel) {
  // The scroll threshold. Taken from |KScrollOffsetThreshold| in
  // scrollable_shelf_view.cc.
  constexpr int scroll_threshold = 20;
  AddAppShortcutsUntilOverflow();

  ASSERT_EQ(ScrollableShelfView::kShowRightArrowButton,
            scrollable_shelf_view_->layout_strategy_for_test());

  // Do a mousewheel scroll with a positive offset bigger than the scroll
  // threshold to scroll forward. Unlike touchpad scrolls, mousewheel scrolls
  // can only be along the cross axis.
  GetEventGenerator()->MoveMouseTo(
      scrollable_shelf_view_->GetBoundsInScreen().CenterPoint());
  GetEventGenerator()->MoveMouseWheel(0, -(scroll_threshold + 1));
  ASSERT_EQ(ScrollableShelfView::kShowLeftArrowButton,
            scrollable_shelf_view_->layout_strategy_for_test());

  // Do a mousewheel scroll with a negative offset bigger than the scroll
  // threshold to scroll backwards.
  GetEventGenerator()->MoveMouseWheel(0, scroll_threshold + 1);
  ASSERT_EQ(ScrollableShelfView::kShowRightArrowButton,
            scrollable_shelf_view_->layout_strategy_for_test());

  // Do a mousewheel scroll with an offset smaller than the scroll
  // threshold should be ignored.
  GetEventGenerator()->MoveMouseWheel(0, scroll_threshold);
  ASSERT_EQ(ScrollableShelfView::kShowRightArrowButton,
            scrollable_shelf_view_->layout_strategy_for_test());

  GetEventGenerator()->MoveMouseWheel(0, -scroll_threshold);
  ASSERT_EQ(ScrollableShelfView::kShowRightArrowButton,
            scrollable_shelf_view_->layout_strategy_for_test());
}

// Verifies that the shelf is scrolled to show the pinned app after pinning.
TEST_F(ScrollableShelfViewTest, FeedbackForAppPinning) {
  AddAppShortcutsUntilOverflow();
  ASSERT_EQ(ScrollableShelfView::kShowRightArrowButton,
            scrollable_shelf_view_->layout_strategy_for_test());

  // Pins the icons of running apps to the shelf.
  const int num = shelf_view_->view_model()->view_size();
  for (int i = 0; i < 2 * num; i++)
    AddAppShortcut(ShelfItemType::TYPE_APP);

  // Emulates the process that user pins another app icon. Icons of pinned apps
  // are placed before those of running apps. So the icon should be located on
  // the second page.
  ShelfModel::ScopedUserTriggeredMutation user_triggered(
      scrollable_shelf_view_->shelf_view()->model());
  ShelfID shelf_id = AddAppShortcut();
  const int view_index =
      shelf_view_->model()->ItemIndexByAppID(shelf_id.app_id);
  ASSERT_EQ(view_index, num);

  // Scrolls the shelf to show the pinned app. Expects that the shelf is
  // scrolled to the correct page.
  EXPECT_LT(view_index, scrollable_shelf_view_->last_tappable_app_index());
  EXPECT_GT(view_index, scrollable_shelf_view_->first_tappable_app_index());
  EXPECT_EQ(ScrollableShelfView::kShowButtons,
            scrollable_shelf_view_->layout_strategy_for_test());
}

// Verifies that removing a shelf icon by mouse works as expected on scrollable
// shelf (see https://crbug.com/1033967).
TEST_F(ScrollableShelfViewTest, RipOffShelfItem) {
  AddAppShortcutsUntilOverflow();
  ASSERT_EQ(ScrollableShelfView::kShowRightArrowButton,
            scrollable_shelf_view_->layout_strategy_for_test());

  views::ViewModel* view_model = shelf_view_->view_model();
  const gfx::Rect first_tappable_view_bounds =
      view_model->view_at(scrollable_shelf_view_->first_tappable_app_index())
          ->GetBoundsInScreen();

  const gfx::Point drag_start_location =
      first_tappable_view_bounds.CenterPoint();
  const gfx::Point drag_end_location = gfx::Point(
      drag_start_location.x(),
      drag_start_location.y() - 3 * ShelfConfig::Get()->shelf_size());

  // Drags an icon off the shelf to remove it.
  GetEventGenerator()->MoveMouseTo(drag_start_location);
  GetEventGenerator()->PressLeftButton();
  GetEventGenerator()->MoveMouseTo(drag_end_location);
  GetEventGenerator()->ReleaseLeftButton();

  // Expects that the scrollable shelf has the correct layout strategy.
  EXPECT_EQ(ScrollableShelfView::kNotShowArrowButtons,
            scrollable_shelf_view_->layout_strategy_for_test());
}

}  // namespace ash
