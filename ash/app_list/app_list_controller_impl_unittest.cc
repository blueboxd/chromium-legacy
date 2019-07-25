// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/app_list/app_list_controller_impl.h"

#include "ash/app_list/app_list_metrics.h"
#include "ash/app_list/views/app_list_main_view.h"
#include "ash/app_list/views/app_list_view.h"
#include "ash/app_list/views/contents_view.h"
#include "ash/app_list/views/expand_arrow_view.h"
#include "ash/app_list/views/search_box_view.h"
#include "ash/home_screen/home_launcher_gesture_handler.h"
#include "ash/home_screen/home_screen_controller.h"
#include "ash/ime/ime_controller.h"
#include "ash/ime/test_ime_controller_client.h"
#include "ash/keyboard/keyboard_controller_impl.h"
#include "ash/public/cpp/app_list/app_list_config.h"
#include "ash/public/cpp/presentation_time_recorder.h"
#include "ash/public/cpp/shelf_types.h"
#include "ash/shelf/shelf.h"
#include "ash/shelf/shelf_widget.h"
#include "ash/shell.h"
#include "ash/system/unified/unified_system_tray_test_api.h"
#include "ash/test/ash_test_base.h"
#include "ash/wm/tablet_mode/tablet_mode_controller.h"
#include "ash/wm/window_state.h"
#include "ash/wm/window_util.h"
#include "base/strings/string16.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/metrics/histogram_tester.h"
#include "ui/events/test/event_generator.h"
#include "ui/message_center/message_center.h"
#include "ui/message_center/views/message_popup_view.h"
#include "ui/views/controls/textfield/textfield_test_api.h"

namespace ash {

namespace {

using ::app_list::kAppListResultLaunchIndexAndQueryLength;
using ::app_list::kAppListTileLaunchIndexAndQueryLength;
using ::app_list::SearchResultLaunchLocation;

bool IsTabletMode() {
  return Shell::Get()->tablet_mode_controller()->InTabletMode();
}

app_list::AppListView* GetAppListView() {
  return Shell::Get()->app_list_controller()->presenter()->GetView();
}

bool GetExpandArrowViewVisibility() {
  return GetAppListView()
      ->app_list_main_view()
      ->contents_view()
      ->expand_arrow_view()
      ->GetVisible();
}

app_list::SearchBoxView* GetSearchBoxView() {
  return GetAppListView()
      ->app_list_main_view()
      ->contents_view()
      ->GetSearchBoxView();
}

aura::Window* GetVirtualKeyboardWindow() {
  return Shell::Get()
      ->keyboard_controller()
      ->keyboard_ui_controller()
      ->GetKeyboardWindow();
}

void ShowAppListNow() {
  Shell::Get()->app_list_controller()->presenter()->Show(
      display::Screen::GetScreen()->GetPrimaryDisplay().id(),
      base::TimeTicks::Now());
}

void DismissAppListNow() {
  Shell::Get()->app_list_controller()->presenter()->Dismiss(
      base::TimeTicks::Now());
}

aura::Window* GetAppListViewNativeWindow() {
  return GetAppListView()->GetWidget()->GetNativeView();
}

void SetSearchText(AppListControllerImpl* controller, const std::string& text) {
  controller->GetSearchModel()->search_box()->Update(base::ASCIIToUTF16(text),
                                                     false);
}

}  // namespace

class AppListControllerImplTest : public AshTestBase {
 public:
  AppListControllerImplTest() = default;
  ~AppListControllerImplTest() override = default;

  std::unique_ptr<aura::Window> CreateTestWindow() {
    return AshTestBase::CreateTestWindow(gfx::Rect(0, 0, 400, 400));
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(AppListControllerImplTest);
};

// Tests that the AppList hides when shelf alignment changes. This necessary
// because the AppList is shown with certain assumptions based on shelf
// orientation.
TEST_F(AppListControllerImplTest, AppListHiddenWhenShelfAlignmentChanges) {
  Shelf* const shelf = AshTestBase::GetPrimaryShelf();
  shelf->SetAlignment(ash::ShelfAlignment::SHELF_ALIGNMENT_BOTTOM);

  const std::vector<ash::ShelfAlignment> alignments(
      {SHELF_ALIGNMENT_LEFT, SHELF_ALIGNMENT_RIGHT, SHELF_ALIGNMENT_BOTTOM});
  for (ash::ShelfAlignment alignment : alignments) {
    ShowAppListNow();
    EXPECT_TRUE(Shell::Get()->app_list_controller()->presenter()->IsVisible());
    shelf->SetAlignment(alignment);
    EXPECT_EQ(AppListViewState::kClosed, GetAppListView()->app_list_state());
  }
}

// Hide the expand arrow view in tablet mode when there is no activatable window
// (see https://crbug.com/923089).
TEST_F(AppListControllerImplTest, UpdateExpandArrowViewVisibility) {
  // Turn on the tablet mode.
  Shell::Get()->tablet_mode_controller()->SetEnabledForTest(true);
  EXPECT_TRUE(IsTabletMode());

  // No activatable windows. So hide the expand arrow view.
  EXPECT_FALSE(GetExpandArrowViewVisibility());

  std::unique_ptr<aura::Window> w1(CreateTestWindow());
  std::unique_ptr<aura::Window> w2(CreateTestWindow());

  // Activate w1 then press home launcher button. Expand arrow view should show
  // because w1 still exists.
  wm::ActivateWindow(w1.get());
  Shell::Get()
      ->home_screen_controller()
      ->home_launcher_gesture_handler()
      ->ShowHomeLauncher(display::Screen::GetScreen()->GetPrimaryDisplay());
  EXPECT_EQ(WindowStateType::kMinimized,
            WindowState::Get(w1.get())->GetStateType());
  EXPECT_TRUE(GetExpandArrowViewVisibility());

  // Activate w2 then close w1. w2 still exists so expand arrow view shows.
  wm::ActivateWindow(w2.get());
  w1.reset();
  EXPECT_TRUE(GetExpandArrowViewVisibility());

  // No activatable windows. Hide the expand arrow view.
  w2.reset();
  EXPECT_FALSE(GetExpandArrowViewVisibility());
}

// In clamshell mode, when the AppListView's bottom is on the display edge
// and app list state is HALF, the rounded corners should be hidden
// (https://crbug.com/942084).
TEST_F(AppListControllerImplTest, HideRoundingCorners) {
  Shell::Get()->keyboard_controller()->SetEnableFlag(
      keyboard::KeyboardEnableFlag::kShelfEnabled);

  // Show the app list view and click on the search box with mouse. So the
  // VirtualKeyboard is shown.
  ShowAppListNow();
  GetSearchBoxView()->SetSearchBoxActive(true, ui::ET_MOUSE_PRESSED);

  // Wait until the virtual keyboard shows on the screen.
  base::RunLoop().RunUntilIdle();
  EXPECT_TRUE(GetVirtualKeyboardWindow()->IsVisible());

  // Test the following things:
  // (1) AppListView is at the top of the screen.
  // (2) AppListView's state is HALF.
  // (3) AppListBackgroundShield is translated to hide the rounded corners.
  aura::Window* native_window = GetAppListView()->GetWidget()->GetNativeView();
  gfx::Rect app_list_screen_bounds = native_window->GetBoundsInScreen();
  EXPECT_EQ(0, app_list_screen_bounds.y());
  EXPECT_EQ(ash::AppListViewState::kHalf, GetAppListView()->app_list_state());
  gfx::Transform expected_transform;
  expected_transform.Translate(
      0, -app_list::AppListConfig::instance().background_radius());
  EXPECT_EQ(
      expected_transform,
      GetAppListView()->GetAppListBackgroundShieldForTest()->GetTransform());

  // Set the search box inactive and wait until the virtual keyboard is hidden.
  GetSearchBoxView()->SetSearchBoxActive(false, ui::ET_MOUSE_PRESSED);
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(nullptr, GetVirtualKeyboardWindow());

  // Test that the rounded corners should show again.
  expected_transform = gfx::Transform();
  EXPECT_EQ(
      expected_transform,
      GetAppListView()->GetAppListBackgroundShieldForTest()->GetTransform());
}

// Verify that when the emoji panel shows and AppListView is in Peeking state,
// AppListView's rounded corners should be hidden (see https://crbug.com/950468)
TEST_F(AppListControllerImplTest, HideRoundingCornersWhenEmojiShows) {
  // Set IME client. Otherwise the emoji panel is unable to show.
  ImeController* ime_controller = Shell::Get()->ime_controller();
  TestImeControllerClient client;
  ime_controller->SetClient(client.CreateInterfacePtr());

  // Show the app list view and right-click on the search box with mouse. So the
  // text field's context menu shows.
  ShowAppListNow();
  app_list::SearchBoxView* search_box_view =
      GetAppListView()->app_list_main_view()->search_box_view();
  gfx::Point center_point = search_box_view->GetBoundsInScreen().CenterPoint();
  ui::test::EventGenerator* event_generator = GetEventGenerator();
  event_generator->MoveMouseTo(center_point);
  event_generator->ClickRightButton();

  // Expect that the first item in the context menu should be "Emoji". Show the
  // emoji panel.
  auto text_field_api =
      std::make_unique<views::TextfieldTestApi>(search_box_view->search_box());
  ASSERT_EQ("Emoji",
            base::UTF16ToUTF8(
                text_field_api->context_menu_contents()->GetLabelAt(0)));
  text_field_api->context_menu_contents()->ActivatedAt(0);

  // Wait for enough time. Then expect that AppListView is pushed up.
  base::RunLoop().RunUntilIdle();
  ASSERT_EQ(gfx::Point(0, 0), GetAppListView()->GetBoundsInScreen().origin());

  // AppListBackgroundShield is translated to hide the rounded corners.
  gfx::Transform expected_transform;
  expected_transform.Translate(
      0, -app_list::AppListConfig::instance().background_radius());
  EXPECT_EQ(
      expected_transform,
      GetAppListView()->GetAppListBackgroundShieldForTest()->GetTransform());
}

// Verifies that in clamshell mode the bounds of AppListView are correct when
// the AppListView is in PEEKING state and the virtual keyboard is enabled (see
// https://crbug.com/944233).
TEST_F(AppListControllerImplTest, CheckAppListViewBoundsWhenVKeyboardEnabled) {
  Shell::Get()->keyboard_controller()->SetEnableFlag(
      keyboard::KeyboardEnableFlag::kShelfEnabled);

  // Show the AppListView and click on the search box with mouse. So the
  // VirtualKeyboard is shown. Wait until the virtual keyboard shows.
  ShowAppListNow();
  GetSearchBoxView()->SetSearchBoxActive(true, ui::ET_MOUSE_PRESSED);
  base::RunLoop().RunUntilIdle();
  EXPECT_TRUE(GetVirtualKeyboardWindow()->IsVisible());

  // Hide the AppListView. Wait until the virtual keyboard is hidden as well.
  DismissAppListNow();
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(nullptr, GetVirtualKeyboardWindow());

  // Show the AppListView again. Check the following things:
  // (1) Virtual keyboard does not show.
  // (2) AppListView is in PEEKING state.
  // (3) AppListView's bounds are the same as the preferred bounds for
  // the PEEKING state.
  ShowAppListNow();
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(ash::AppListViewState::kPeeking,
            GetAppListView()->app_list_state());
  EXPECT_EQ(nullptr, GetVirtualKeyboardWindow());
  EXPECT_EQ(GetAppListView()->GetPreferredWidgetBoundsForState(
                ash::AppListViewState::kPeeking),
            GetAppListViewNativeWindow()->bounds());
}

// Verifies that in tablet mode, the AppListView has correct bounds when the
// virtual keyboard is dismissed (see https://crbug.com/944133).
TEST_F(AppListControllerImplTest, CheckAppListViewBoundsWhenDismissVKeyboard) {
  Shell::Get()->keyboard_controller()->SetEnableFlag(
      keyboard::KeyboardEnableFlag::kShelfEnabled);

  // Show the AppListView and click on the search box with mouse so the
  // VirtualKeyboard is shown. Wait until the virtual keyboard shows.
  ShowAppListNow();
  GetSearchBoxView()->SetSearchBoxActive(true, ui::ET_MOUSE_PRESSED);
  base::RunLoop().RunUntilIdle();
  EXPECT_TRUE(GetVirtualKeyboardWindow()->IsVisible());

  // Turn on the tablet mode. The virtual keyboard should still show.
  Shell::Get()->tablet_mode_controller()->SetEnabledForTest(true);
  EXPECT_TRUE(IsTabletMode());
  EXPECT_TRUE(GetVirtualKeyboardWindow()->IsVisible());

  // Close the virtual keyboard. Wait until it is hidden.
  Shell::Get()->keyboard_controller()->HideKeyboard(HideReason::kUser);
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(nullptr, GetVirtualKeyboardWindow());

  // Check the following things:
  // (1) AppListView's state is FULLSCREEN_SEARCH
  // (2) AppListView's bounds are the same as the preferred bounds for
  // the FULLSCREEN_SEARCH state.
  EXPECT_EQ(ash::AppListViewState::kFullscreenSearch,
            GetAppListView()->app_list_state());
  EXPECT_EQ(GetAppListView()->GetPreferredWidgetBoundsForState(
                ash::AppListViewState::kFullscreenSearch),
            GetAppListViewNativeWindow()->bounds());
}

// Verifies that closing notification by gesture should not dismiss the AppList.
// (see https://crbug.com/948344)
TEST_F(AppListControllerImplTest, CloseNotificationWithAppListShown) {
  ShowAppListNow();

  // Add one notification.
  ASSERT_EQ(
      0u, message_center::MessageCenter::Get()->GetPopupNotifications().size());
  const std::string notification_id("id");
  const std::string notification_title("title");
  message_center::MessageCenter::Get()->AddNotification(
      std::make_unique<message_center::Notification>(
          message_center::NOTIFICATION_TYPE_BASE_FORMAT, notification_id,
          base::UTF8ToUTF16(notification_title),
          base::UTF8ToUTF16("test message"), gfx::Image(),
          base::string16() /* display_source */, GURL(),
          message_center::NotifierId(), message_center::RichNotificationData(),
          new message_center::NotificationDelegate()));
  base::RunLoop().RunUntilIdle();
  ASSERT_EQ(
      1u, message_center::MessageCenter::Get()->GetPopupNotifications().size());

  // Calculate the drag start point and end point.
  UnifiedSystemTrayTestApi test_api(GetPrimaryUnifiedSystemTray());
  message_center::MessagePopupView* popup_view =
      test_api.GetPopupViewForNotificationID(notification_id);
  ASSERT_TRUE(popup_view);
  gfx::Rect bounds_in_screen = popup_view->GetBoundsInScreen();
  const gfx::Point drag_start = bounds_in_screen.left_center();
  const gfx::Point drag_end = bounds_in_screen.right_center();

  // Swipe away notification by gesture. Verifies that AppListView still shows.
  ui::test::EventGenerator* event_generator = GetEventGenerator();
  event_generator->GestureScrollSequence(
      drag_start, drag_end, base::TimeDelta::FromMicroseconds(500), 10);
  base::RunLoop().RunUntilIdle();
  EXPECT_TRUE(GetAppListView());
  EXPECT_EQ(
      0u, message_center::MessageCenter::Get()->GetPopupNotifications().size());
}

// Tests that full screen apps list opens when user touches on or near the
// expand view arrow. (see https://crbug.com/906858)
TEST_F(AppListControllerImplTest,
       EnterFullScreenModeAfterTappingNearExpandArrow) {
  ShowAppListNow();
  ASSERT_EQ(ash::AppListViewState::kPeeking,
            GetAppListView()->app_list_state());

  // Get in screen bounds of arrow
  const gfx::Rect expand_arrow = GetAppListView()
                                     ->app_list_main_view()
                                     ->contents_view()
                                     ->expand_arrow_view()
                                     ->GetBoundsInScreen();

  // Tap expand arrow icon and check that full screen apps view is entered.
  ui::test::EventGenerator* event_generator = GetEventGenerator();
  event_generator->GestureTapAt(expand_arrow.CenterPoint());
  ASSERT_EQ(ash::AppListViewState::kFullscreenAllApps,
            GetAppListView()->app_list_state());

  // Hide the AppListView. Wait until animation is finished
  DismissAppListNow();
  base::RunLoop().RunUntilIdle();

  // Re-enter peeking mode and test that tapping near (but not directly on)
  // the expand arrow icon still brings up full app list view.
  ShowAppListNow();
  ASSERT_EQ(ash::AppListViewState::kPeeking,
            GetAppListView()->app_list_state());

  event_generator->GestureTapAt(
      gfx::Point(expand_arrow.top_right().x(), expand_arrow.top_right().y()));

  ASSERT_EQ(ash::AppListViewState::kFullscreenAllApps,
            GetAppListView()->app_list_state());
}

class AppListControllerImplMetricsTest : public AshTestBase {
 public:
  AppListControllerImplMetricsTest() = default;
  ~AppListControllerImplMetricsTest() override = default;

  void SetUp() override {
    AshTestBase::SetUp();
    controller_ = ash::Shell::Get()->app_list_controller();
    ash::PresentationTimeRecorder::SetReportPresentationTimeImmediatelyForTest(
        true);
  }

  void TearDown() override {
    ash::PresentationTimeRecorder::SetReportPresentationTimeImmediatelyForTest(
        false);
    AshTestBase::TearDown();
  }

  AppListControllerImpl* controller_;
  const base::HistogramTester histogram_tester_;

 private:
  DISALLOW_COPY_AND_ASSIGN(AppListControllerImplMetricsTest);
};

TEST_F(AppListControllerImplMetricsTest, LogSingleResultListClick) {
  histogram_tester_.ExpectTotalCount(kAppListResultLaunchIndexAndQueryLength,
                                     0);
  SetSearchText(controller_, "");
  controller_->LogResultLaunchHistogram(SearchResultLaunchLocation::kResultList,
                                        4);
  histogram_tester_.ExpectUniqueSample(kAppListResultLaunchIndexAndQueryLength,
                                       4, 1);
}

TEST_F(AppListControllerImplMetricsTest, LogSingleTileListClick) {
  histogram_tester_.ExpectTotalCount(kAppListTileLaunchIndexAndQueryLength, 0);
  SetSearchText(controller_, "aaaa");
  controller_->LogResultLaunchHistogram(SearchResultLaunchLocation::kTileList,
                                        4);
  histogram_tester_.ExpectUniqueSample(kAppListTileLaunchIndexAndQueryLength,
                                       32, 1);
}

TEST_F(AppListControllerImplMetricsTest, LogOneClickInEveryBucket) {
  histogram_tester_.ExpectTotalCount(kAppListResultLaunchIndexAndQueryLength,
                                     0);
  for (int query_length = 0; query_length < 11; ++query_length) {
    const std::string query(query_length, 'a');
    for (int click_index = 0; click_index < 7; ++click_index) {
      SetSearchText(controller_, query);
      controller_->LogResultLaunchHistogram(
          SearchResultLaunchLocation::kResultList, click_index);
    }
  }

  histogram_tester_.ExpectTotalCount(kAppListResultLaunchIndexAndQueryLength,
                                     77);
  for (int query_length = 0; query_length < 11; ++query_length) {
    for (int click_index = 0; click_index < 7; ++click_index) {
      histogram_tester_.ExpectBucketCount(
          kAppListResultLaunchIndexAndQueryLength,
          7 * query_length + click_index, 1);
    }
  }
}

TEST_F(AppListControllerImplMetricsTest, LogManyClicksInOneBucket) {
  histogram_tester_.ExpectTotalCount(kAppListTileLaunchIndexAndQueryLength, 0);
  SetSearchText(controller_, "aaaa");
  for (int i = 0; i < 50; ++i)
    controller_->LogResultLaunchHistogram(SearchResultLaunchLocation::kTileList,
                                          4);
  histogram_tester_.ExpectUniqueSample(kAppListTileLaunchIndexAndQueryLength,
                                       32, 50);
}

// Verifies that the PresentationTimeRecorder works correctly for the home
// launcher gesture drag in tablet mode (https://crbug.com/947105).
TEST_F(AppListControllerImplMetricsTest,
       PresentationTimeRecordedForDragInTabletMode) {
  // Wait until the construction of TabletModeController finishes.
  base::RunLoop().RunUntilIdle();

  // Turn on the tablet mode.
  Shell::Get()->tablet_mode_controller()->SetEnabledForTest(true);
  EXPECT_TRUE(IsTabletMode());

  // Create a window then press the home launcher button. Expect that |w| is
  // hidden.
  std::unique_ptr<aura::Window> w(
      AshTestBase::CreateTestWindow(gfx::Rect(0, 0, 400, 400)));
  Shell::Get()
      ->home_screen_controller()
      ->home_launcher_gesture_handler()
      ->ShowHomeLauncher(display::Screen::GetScreen()->GetPrimaryDisplay());
  EXPECT_FALSE(w->IsVisible());
  EXPECT_EQ(AppListViewState::kFullscreenAllApps,
            GetAppListView()->app_list_state());

  int delta_y = 1;
  gfx::Point start =
      GetAppListView()->GetWidget()->GetWindowBoundsInScreen().top_right();
  base::TimeTicks timestamp = base::TimeTicks::Now();

  // Emulate to drag the launcher downward.
  // Send SCROLL_START event. Check the presentation metrics values.
  ui::GestureEvent start_event = ui::GestureEvent(
      start.x(), start.y(), ui::EF_NONE, timestamp,
      ui::GestureEventDetails(ui::ET_GESTURE_SCROLL_BEGIN, 0, delta_y));
  GetAppListView()->OnGestureEvent(&start_event);
  histogram_tester_.ExpectTotalCount(
      "Apps.StateTransition.Drag.PresentationTime.TabletMode", 0);
  histogram_tester_.ExpectTotalCount(
      "Apps.StateTransition.Drag.PresentationTime.MaxLatency.TabletMode", 0);

  // Send SCROLL_UPDATE event. Check the presentation metrics values.
  timestamp += base::TimeDelta::FromMilliseconds(25);
  delta_y += 20;
  start.Offset(0, 1);
  ui::GestureEvent update_event = ui::GestureEvent(
      start.x(), start.y(), ui::EF_NONE, timestamp,
      ui::GestureEventDetails(ui::ET_GESTURE_SCROLL_UPDATE, 0, delta_y));
  GetAppListView()->OnGestureEvent(&update_event);
  histogram_tester_.ExpectTotalCount(
      "Apps.StateTransition.Drag.PresentationTime.TabletMode", 1);
  histogram_tester_.ExpectTotalCount(
      "Apps.StateTransition.Drag.PresentationTime.MaxLatency.TabletMode", 0);

  // Send SCROLL_END event. Check the presentation metrics values.
  timestamp += base::TimeDelta::FromMilliseconds(25);
  start.Offset(0, 1);
  ui::GestureEvent end_event =
      ui::GestureEvent(start.x(), start.y() + delta_y, ui::EF_NONE, timestamp,
                       ui::GestureEventDetails(ui::ET_GESTURE_END));
  GetAppListView()->OnGestureEvent(&end_event);
  histogram_tester_.ExpectTotalCount(
      "Apps.StateTransition.Drag.PresentationTime.TabletMode", 1);
  histogram_tester_.ExpectTotalCount(
      "Apps.StateTransition.Drag.PresentationTime.MaxLatency.TabletMode", 1);

  // After the gesture scroll event ends, the window shows.
  EXPECT_TRUE(w->IsVisible());
  ASSERT_TRUE(IsTabletMode());
}

// One edge case may do harm to the presentation metrics reporter for tablet
// mode: the user may keep pressing on launcher while exiting the tablet mode by
// rotating the lid. In this situation, OnHomeLauncherDragEnd is not triggered.
// It is handled correctly now because the AppListView is always closed after
// exiting the tablet mode. But it still has potential risk to break in future.
// Write this test case for precaution (https://crbug.com/947105).
TEST_F(AppListControllerImplMetricsTest,
       PresentationMetricsForTabletNotRecordedInClamshell) {
  // Wait until the construction of TabletModeController finishes.
  base::RunLoop().RunUntilIdle();

  // Turn on the tablet mode.
  Shell::Get()->tablet_mode_controller()->SetEnabledForTest(true);
  EXPECT_TRUE(IsTabletMode());

  // Create a window then press the home launcher button. Expect that |w| is
  // hidden.
  std::unique_ptr<aura::Window> w(
      AshTestBase::CreateTestWindow(gfx::Rect(0, 0, 400, 400)));
  Shell::Get()
      ->home_screen_controller()
      ->home_launcher_gesture_handler()
      ->ShowHomeLauncher(display::Screen::GetScreen()->GetPrimaryDisplay());
  EXPECT_FALSE(w->IsVisible());
  EXPECT_EQ(AppListViewState::kFullscreenAllApps,
            GetAppListView()->app_list_state());

  gfx::Point start =
      GetAppListView()->GetWidget()->GetWindowBoundsInScreen().top_right();
  base::TimeTicks timestamp = base::TimeTicks::Now();

  // Emulate to drag the launcher downward.
  // Send SCROLL_START event. Check the presentation metrics values.
  ui::GestureEvent start_event = ui::GestureEvent(
      start.x(), start.y(), ui::EF_NONE, timestamp,
      ui::GestureEventDetails(ui::ET_GESTURE_SCROLL_BEGIN, 0, 1));
  GetAppListView()->OnGestureEvent(&start_event);

  // Turn off the tablet mode before scrolling is finished.
  Shell::Get()->tablet_mode_controller()->SetEnabledForTest(false);
  EXPECT_FALSE(IsTabletMode());
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(AppListViewState::kClosed, GetAppListView()->app_list_state());

  // Check metrics initial values.
  histogram_tester_.ExpectTotalCount(
      "Apps.StateTransition.Drag.PresentationTime.TabletMode", 0);
  histogram_tester_.ExpectTotalCount(
      "Apps.StateTransition.Drag.PresentationTime.MaxLatency.TabletMode", 0);

  // Emulate to drag launcher from shelf. Then verifies the following things:
  // (1) Metrics values for tablet mode are not recorded.
  // (2) Metrics values for clamshell mode are recorded correctly.
  gfx::Rect shelf_bounds =
      GetPrimaryShelf()->shelf_widget()->GetWindowBoundsInScreen();
  shelf_bounds.Intersect(
      display::Screen::GetScreen()->GetPrimaryDisplay().bounds());
  gfx::Point shelf_center = shelf_bounds.CenterPoint();
  gfx::Point target_point =
      display::Screen::GetScreen()->GetPrimaryDisplay().bounds().CenterPoint();
  ui::test::EventGenerator* generator = GetEventGenerator();
  generator->GestureScrollSequence(shelf_center, target_point,
                                   base::TimeDelta::FromMicroseconds(500), 1);
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(AppListViewState::kFullscreenAllApps,
            GetAppListView()->app_list_state());
  histogram_tester_.ExpectTotalCount(
      "Apps.StateTransition.Drag.PresentationTime.TabletMode", 0);
  histogram_tester_.ExpectTotalCount(
      "Apps.StateTransition.Drag.PresentationTime.MaxLatency.TabletMode", 0);

  // AppListView::UpdateYPositionAndOpacity is triggered by
  // ShelfLayoutManager::StartGestureDrag and
  // ShelfLayoutManager::UpdateGestureDrag. Note that scrolling step of event
  // generator is 1. So the expected value is 2.
  histogram_tester_.ExpectTotalCount(
      "Apps.StateTransition.Drag.PresentationTime.ClamshellMode", 2);

  histogram_tester_.ExpectTotalCount(
      "Apps.StateTransition.Drag.PresentationTime.MaxLatency.ClamshellMode", 1);
}

}  // namespace ash
