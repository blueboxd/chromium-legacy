// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>
#include <vector>

#include "ash/app_list/app_list_controller_impl.h"
#include "ash/app_list/model/search/search_model.h"
#include "ash/app_list/test/app_list_test_helper.h"
#include "ash/app_list/test/app_list_test_model.h"
#include "ash/app_list/views/app_list_item_view.h"
#include "ash/app_list/views/app_list_main_view.h"
#include "ash/app_list/views/app_list_view.h"
#include "ash/app_list/views/apps_container_view.h"
#include "ash/app_list/views/apps_grid_view.h"
#include "ash/app_list/views/contents_view.h"
#include "ash/app_list/views/search_result_container_view.h"
#include "ash/app_list/views/search_result_page_view.h"
#include "ash/app_list/views/search_result_tile_item_view.h"
#include "ash/app_list/views/suggestion_chip_container_view.h"
#include "ash/public/cpp/app_list/app_list_types.h"
#include "ash/public/cpp/shelf_item_delegate.h"
#include "ash/public/cpp/shelf_model.h"
#include "ash/shelf/shelf.h"
#include "ash/shelf/shelf_view.h"
#include "ash/shelf/shelf_view_test_api.h"
#include "ash/shell.h"
#include "ash/test/ash_test_base.h"
#include "ash/wm/tablet_mode/tablet_mode_controller.h"
#include "base/test/metrics/histogram_tester.h"
#include "ui/display/screen.h"
#include "ui/events/test/event_generator.h"

namespace ash {

namespace {

constexpr int kBrowserAppIndexOnShelf = 0;

// A test shelf item delegate that simulates an activated window when a shelf
// item is selected.
class TestShelfItemDelegate : public ShelfItemDelegate {
 public:
  explicit TestShelfItemDelegate(const ShelfID& shelf_id)
      : ShelfItemDelegate(shelf_id) {}

  void ItemSelected(std::unique_ptr<ui::Event> event,
                    int64_t display_id,
                    ash::ShelfLaunchSource source,
                    ItemSelectedCallback callback) override {
    std::move(callback).Run(SHELF_ACTION_WINDOW_ACTIVATED, {});
  }
  void ExecuteCommand(bool from_context_menu,
                      int64_t command_id,
                      int32_t event_flags,
                      int64_t display_id) override {}
  void Close() override {}
};

}  // namespace

int64_t GetPrimaryDisplayId() {
  return display::Screen::GetScreen()->GetPrimaryDisplay().id();
}

// Used to test that app launched metrics are properly recorded.
class AppListAppLaunchedMetricTest : public AshTestBase {
 public:
  AppListAppLaunchedMetricTest() = default;
  ~AppListAppLaunchedMetricTest() override = default;

  void SetUp() override {
    app_list::AppListView::SetShortAnimationForTesting(true);
    AshTestBase::SetUp();

    search_model_ = Shell::Get()->app_list_controller()->GetSearchModel();

    app_list_test_model_ = static_cast<app_list::test::AppListTestModel*>(
        Shell::Get()->app_list_controller()->GetModel());
  }

  void TearDown() override {
    AshTestBase::TearDown();
    app_list::AppListView::SetShortAnimationForTesting(false);
  }

 protected:
  void CreateAndClickShelfItem() {
    // Add shelf item to be launched.
    ShelfItem shelf_item;
    shelf_item.id = ash::ShelfID("app_id");
    shelf_item.type = TYPE_BROWSER_SHORTCUT;
    ShelfModel::Get()->Add(shelf_item);

    // The TestShelfItemDelegate will simulate a window activation after the
    // shelf item is clicked.
    ShelfModel::Get()->SetShelfItemDelegate(
        shelf_item.id, std::make_unique<TestShelfItemDelegate>(shelf_item.id));

    ClickShelfItem();
  }

  void ClickShelfItem() {
    // Get location of the shelf item.
    ShelfViewTestAPI shelf_test_api(
        GetPrimaryShelf()->GetShelfViewForTesting());
    ShelfView* shelf_view = shelf_test_api.shelf_view();
    const views::ViewModel* view_model = shelf_view->view_model_for_test();
    gfx::Point center = view_model->view_at(kBrowserAppIndexOnShelf)
                            ->GetBoundsInScreen()
                            .CenterPoint();

    // Click on the shelf item.
    ui::test::EventGenerator* generator = GetEventGenerator();
    generator->MoveMouseTo(center);
    generator->ClickLeftButton();
    generator->ReleaseLeftButton();
  }

  void PopulateAndLaunchSearchBoxTileItem() {
    // Populate 4 tile items.
    for (size_t i = 0; i < 4; i++) {
      auto search_result = std::make_unique<app_list::SearchResult>();
      search_result->set_display_type(ash::SearchResultDisplayType::kTile);
      search_model_->results()->Add(std::move(search_result));
    }
    GetAppListTestHelper()->WaitUntilIdle();

    app_list::SearchResultContainerView* search_result_container_view =
        Shell::Get()
            ->app_list_controller()
            ->presenter()
            ->GetView()
            ->app_list_main_view()
            ->contents_view()
            ->search_results_page_view()
            ->result_container_views()[1];

    // Request focus on the first tile item view.
    search_result_container_view->GetFirstResultView()->RequestFocus();

    // Press return to simulate an app launch from the tile item.
    GetEventGenerator()->PressKey(ui::KeyboardCode::VKEY_RETURN, 0);
  }

  void PopulateAndLaunchSuggestionChip() {
    // Populate 4 suggestion chips.
    for (size_t i = 0; i < 4; i++) {
      auto search_result_chip = std::make_unique<app_list::SearchResult>();
      search_result_chip->set_display_type(
          ash::SearchResultDisplayType::kRecommendation);
      search_model_->results()->Add(std::move(search_result_chip));
    }
    GetAppListTestHelper()->WaitUntilIdle();

    app_list::SearchResultContainerView* suggestions_container_ =
        Shell::Get()
            ->app_list_controller()
            ->presenter()
            ->GetView()
            ->app_list_main_view()
            ->contents_view()
            ->GetAppsContainerView()
            ->suggestion_chip_container_view_for_test();

    // Get focus on the first chip.
    suggestions_container_->children().front()->RequestFocus();
    GetAppListTestHelper()->WaitUntilIdle();

    // Press return to simulate an app launch from the suggestion chip.
    GetEventGenerator()->PressKey(ui::KeyboardCode::VKEY_RETURN, 0);
  }

  void PopulateAndLaunchAppInGrid() {
    // Populate apps in the root app grid.
    app_list_test_model_->PopulateApps(4);

    app_list::AppListView::TestApi test_api(
        Shell::Get()->app_list_controller()->presenter()->GetView());

    // Focus the first item in the root app grid.
    test_api.GetRootAppsGridView()->GetItemViewAt(0)->RequestFocus();

    // Press return to simulate an app launch from a grid item.
    GetEventGenerator()->PressKey(ui::KeyboardCode::VKEY_RETURN, 0);
  }

 private:
  app_list::SearchModel* search_model_ = nullptr;
  app_list::test::AppListTestModel* app_list_test_model_ = nullptr;

  DISALLOW_COPY_AND_ASSIGN(AppListAppLaunchedMetricTest);
};

// Test that the histogram records an app launch from the shelf while the half
// launcher is showing.
TEST_F(AppListAppLaunchedMetricTest, HalfLaunchFromShelf) {
  base::HistogramTester histogram_tester;

  GetAppListTestHelper()->ShowAndRunLoop(GetPrimaryDisplayId());
  GetAppListTestHelper()->CheckState(ash::AppListViewState::kPeeking);

  // Press a letter key, the AppListView should transition to kHalf.
  GetEventGenerator()->PressKey(ui::KeyboardCode::VKEY_H, 0);
  GetAppListTestHelper()->CheckState(ash::AppListViewState::kHalf);

  CreateAndClickShelfItem();
  GetAppListTestHelper()->WaitUntilIdle();

  histogram_tester.ExpectBucketCount(
      "Apps.AppListAppLaunchedV2.Half", AppListLaunchedFrom::kLaunchedFromShelf,
      1 /* Number of times launched from shelf */);
}

// Test that the histogram records an app launch from the search box while the
// half launcher is showing.
TEST_F(AppListAppLaunchedMetricTest, HalfLaunchFromSearchBox) {
  base::HistogramTester histogram_tester;

  GetAppListTestHelper()->ShowAndRunLoop(GetPrimaryDisplayId());
  GetAppListTestHelper()->CheckState(ash::AppListViewState::kPeeking);

  // Press a letter key, the AppListView should transition to kHalf.
  GetEventGenerator()->PressKey(ui::KeyboardCode::VKEY_H, 0);
  GetAppListTestHelper()->CheckState(ash::AppListViewState::kHalf);

  PopulateAndLaunchSearchBoxTileItem();

  histogram_tester.ExpectBucketCount(
      "Apps.AppListAppLaunchedV2.Half",
      AppListLaunchedFrom::kLaunchedFromSearchBox,
      1 /* Number of times launched from search box */);
}

// Test that the histogram records an app launch from the search box while the
// fullscreen search launcher is showing.
TEST_F(AppListAppLaunchedMetricTest, FullscreenSearchLaunchFromSearchBox) {
  base::HistogramTester histogram_tester;
  ui::test::EventGenerator* generator = GetEventGenerator();

  // Press search + shift to transition to kFullscreenAllApps.
  generator->PressKey(ui::KeyboardCode::VKEY_BROWSER_SEARCH, ui::EF_SHIFT_DOWN);
  GetAppListTestHelper()->CheckState(ash::AppListViewState::kFullscreenAllApps);

  // Press a letter key, the AppListView should transition to kFullscreenSearch.
  generator->PressKey(ui::KeyboardCode::VKEY_H, 0);
  GetAppListTestHelper()->CheckState(ash::AppListViewState::kFullscreenSearch);

  PopulateAndLaunchSearchBoxTileItem();

  GetAppListTestHelper()->WaitUntilIdle();
  histogram_tester.ExpectBucketCount(
      "Apps.AppListAppLaunchedV2.FullscreenSearch",
      AppListLaunchedFrom::kLaunchedFromSearchBox,
      1 /* Number of times launched from search box */);
}

// Test that the histogram records an app launch from the shelf while the
// fullscreen search launcher is showing.
TEST_F(AppListAppLaunchedMetricTest, FullscreenSearchLaunchFromShelf) {
  base::HistogramTester histogram_tester;
  ui::test::EventGenerator* generator = GetEventGenerator();

  // Press search + shift to transition to kFullscreenAllApps.
  generator->PressKey(ui::KeyboardCode::VKEY_BROWSER_SEARCH, ui::EF_SHIFT_DOWN);
  GetAppListTestHelper()->CheckState(ash::AppListViewState::kFullscreenAllApps);

  // Press a letter key, the AppListView should transition to kFullscreenSearch.
  generator->PressKey(ui::KeyboardCode::VKEY_H, 0);
  GetAppListTestHelper()->CheckState(ash::AppListViewState::kFullscreenSearch);

  CreateAndClickShelfItem();

  histogram_tester.ExpectBucketCount(
      "Apps.AppListAppLaunchedV2.FullscreenSearch",
      AppListLaunchedFrom::kLaunchedFromShelf,
      1 /* Number of times launched from shelf */);
}

// Test that the histogram records an app launch from a suggestion chip while
// the fullscreen all apps launcher is showing.
TEST_F(AppListAppLaunchedMetricTest, FullscreenAllAppsLaunchFromChip) {
  base::HistogramTester histogram_tester;

  // Press search + shift to transition to kFullscreenAllApps.
  GetEventGenerator()->PressKey(ui::KeyboardCode::VKEY_BROWSER_SEARCH,
                                ui::EF_SHIFT_DOWN);
  GetAppListTestHelper()->CheckState(ash::AppListViewState::kFullscreenAllApps);

  PopulateAndLaunchSuggestionChip();

  histogram_tester.ExpectBucketCount(
      "Apps.AppListAppLaunchedV2.FullscreenAllApps",
      AppListLaunchedFrom::kLaunchedFromSuggestionChip,
      1 /* Number of times launched from chip */);
}

// Test that the histogram records an app launch from the app grid while the
// fullscreen all apps launcher is showing.
TEST_F(AppListAppLaunchedMetricTest, FullscreenAllAppsLaunchFromGrid) {
  base::HistogramTester histogram_tester;

  // Press search + shift to transition to kFullscreenAllApps.
  GetEventGenerator()->PressKey(ui::KeyboardCode::VKEY_BROWSER_SEARCH,
                                ui::EF_SHIFT_DOWN);
  GetAppListTestHelper()->CheckState(ash::AppListViewState::kFullscreenAllApps);

  PopulateAndLaunchAppInGrid();

  histogram_tester.ExpectBucketCount(
      "Apps.AppListAppLaunchedV2.FullscreenAllApps",
      AppListLaunchedFrom::kLaunchedFromGrid,
      1 /* Number of times launched from grid */);
}

// Test that the histogram records an app launch from the shelf while the
// fullscreen all apps launcher is showing.
TEST_F(AppListAppLaunchedMetricTest, FullscreenAllAppsLaunchFromShelf) {
  base::HistogramTester histogram_tester;

  // Press search + shift to transition to kFullscreenAllApps.
  GetEventGenerator()->PressKey(ui::KeyboardCode::VKEY_BROWSER_SEARCH,
                                ui::EF_SHIFT_DOWN);
  GetAppListTestHelper()->CheckState(ash::AppListViewState::kFullscreenAllApps);

  CreateAndClickShelfItem();

  histogram_tester.ExpectBucketCount(
      "Apps.AppListAppLaunchedV2.FullscreenAllApps",
      AppListLaunchedFrom::kLaunchedFromShelf,
      1 /* Number of times launched from shelf */);
}

// Test that the histogram records an app launch from the shelf while the
// peeking launcher is showing.
TEST_F(AppListAppLaunchedMetricTest, PeekingLaunchFromShelf) {
  base::HistogramTester histogram_tester;

  GetAppListTestHelper()->ShowAndRunLoop(GetPrimaryDisplayId());
  GetAppListTestHelper()->CheckState(ash::AppListViewState::kPeeking);

  CreateAndClickShelfItem();

  histogram_tester.ExpectBucketCount(
      "Apps.AppListAppLaunchedV2.Peeking",
      AppListLaunchedFrom::kLaunchedFromShelf,
      1 /* Number of times launched from shelf */);
}

// Test that the histogram records an app launch from a suggestion chip while
// the peeking launcher is showing.
TEST_F(AppListAppLaunchedMetricTest, PeekingLaunchFromChip) {
  base::HistogramTester histogram_tester;

  GetAppListTestHelper()->ShowAndRunLoop(GetPrimaryDisplayId());
  GetAppListTestHelper()->CheckState(ash::AppListViewState::kPeeking);

  PopulateAndLaunchSuggestionChip();

  histogram_tester.ExpectBucketCount(
      "Apps.AppListAppLaunchedV2.Peeking",
      AppListLaunchedFrom::kLaunchedFromSuggestionChip,
      1 /* Number of times launched from chip */);
}

// Test that the histogram records an app launch from the shelf while the
// launcher is closed.
TEST_F(AppListAppLaunchedMetricTest, ClosedLaunchFromShelf) {
  base::HistogramTester histogram_tester;

  GetAppListTestHelper()->CheckState(ash::AppListViewState::kClosed);

  CreateAndClickShelfItem();

  histogram_tester.ExpectBucketCount(
      "Apps.AppListAppLaunchedV2.Closed",
      AppListLaunchedFrom::kLaunchedFromShelf,
      1 /* Number of times launched from shelf */);

  // Open the launcher to peeking.
  GetEventGenerator()->PressKey(ui::KeyboardCode::VKEY_BROWSER_SEARCH, 0);
  GetAppListTestHelper()->CheckState(ash::AppListViewState::kPeeking);

  // Close launcher back to closed.
  GetEventGenerator()->PressKey(ui::KeyboardCode::VKEY_BROWSER_SEARCH, 0);
  GetAppListTestHelper()->CheckState(ash::AppListViewState::kClosed);

  ClickShelfItem();

  histogram_tester.ExpectBucketCount(
      "Apps.AppListAppLaunchedV2.Closed",
      AppListLaunchedFrom::kLaunchedFromShelf,
      2 /* Number of times launched from shelf */);
}

// Test that the histogram records an app launch from the shelf while the
// homecher all apps state is showing.
TEST_F(AppListAppLaunchedMetricTest, HomecherAllAppsLaunchFromShelf) {
  Shell::Get()->tablet_mode_controller()->SetEnabledForTest(true);
  base::HistogramTester histogram_tester;

  GetAppListTestHelper()->CheckState(ash::AppListViewState::kFullscreenAllApps);

  CreateAndClickShelfItem();

  histogram_tester.ExpectBucketCount(
      "Apps.AppListAppLaunchedV2.HomecherAllApps",
      AppListLaunchedFrom::kLaunchedFromShelf,
      1 /* Number of times launched from shelf */);
}

// Test that the histogram records an app launch from the app grid while the
// homecher all apps state is showing.
TEST_F(AppListAppLaunchedMetricTest, HomecherAllAppsLaunchFromGrid) {
  base::HistogramTester histogram_tester;

  // Enable tablet mode.
  Shell::Get()->tablet_mode_controller()->SetEnabledForTest(true);
  GetAppListTestHelper()->CheckState(ash::AppListViewState::kFullscreenAllApps);

  PopulateAndLaunchAppInGrid();

  histogram_tester.ExpectBucketCount(
      "Apps.AppListAppLaunchedV2.HomecherAllApps",
      AppListLaunchedFrom::kLaunchedFromGrid,
      1 /* Number of times launched from grid */);
}

// Test that the histogram records an app launch from a suggestion chip while
// the homecher all apps state is showing.
TEST_F(AppListAppLaunchedMetricTest, HomecherAllAppsLaunchFromChip) {
  base::HistogramTester histogram_tester;

  GetAppListTestHelper()->WaitUntilIdle();
  // Enable tablet mode.
  Shell::Get()->tablet_mode_controller()->SetEnabledForTest(true);
  GetAppListTestHelper()->CheckState(ash::AppListViewState::kFullscreenAllApps);

  PopulateAndLaunchSuggestionChip();

  histogram_tester.ExpectBucketCount(
      "Apps.AppListAppLaunchedV2.HomecherAllApps",
      AppListLaunchedFrom::kLaunchedFromSuggestionChip,
      1 /* Number of times launched from chip */);
}

// Test that the histogram records an app launch from the shelf while the
// homecher search state is showing.
TEST_F(AppListAppLaunchedMetricTest, HomecherSearchLaunchFromShelf) {
  base::HistogramTester histogram_tester;

  // Enable tablet mode.
  GetAppListTestHelper()->WaitUntilIdle();
  Shell::Get()->tablet_mode_controller()->SetEnabledForTest(true);

  // Press a letter key, the AppListView should transition to kFullscreenSearch.
  GetEventGenerator()->PressKey(ui::KeyboardCode::VKEY_H, 0);
  GetAppListTestHelper()->WaitUntilIdle();
  GetAppListTestHelper()->CheckState(ash::AppListViewState::kFullscreenSearch);

  CreateAndClickShelfItem();

  histogram_tester.ExpectBucketCount(
      "Apps.AppListAppLaunchedV2.HomecherSearch",
      AppListLaunchedFrom::kLaunchedFromShelf,
      1 /* Number of times launched from shelf */);
}

// Test that the histogram records an app launch from the search box while the
// homercher search state is showing.
TEST_F(AppListAppLaunchedMetricTest, HomecherSearchLaunchFromSearchBox) {
  base::HistogramTester histogram_tester;

  // Enable tablet mode.
  GetAppListTestHelper()->WaitUntilIdle();
  Shell::Get()->tablet_mode_controller()->SetEnabledForTest(true);

  // Press a letter key, the AppListView should transition to kFullscreenSearch.
  GetEventGenerator()->PressKey(ui::KeyboardCode::VKEY_H, 0);
  GetAppListTestHelper()->WaitUntilIdle();
  GetAppListTestHelper()->CheckState(ash::AppListViewState::kFullscreenSearch);

  // Populate search box with tile items and launch a tile item.
  PopulateAndLaunchSearchBoxTileItem();

  histogram_tester.ExpectBucketCount(
      "Apps.AppListAppLaunchedV2.HomecherSearch",
      AppListLaunchedFrom::kLaunchedFromSearchBox,
      1 /* Number of times launched from search box */);
}

}  // namespace ash
