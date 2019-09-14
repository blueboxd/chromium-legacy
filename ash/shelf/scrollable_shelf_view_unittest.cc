// Copyright (c) 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/shelf/scrollable_shelf_view.h"

#include "ash/shelf/shelf_test_util.h"
#include "ash/shelf/shelf_tooltip_manager.h"
#include "ash/shelf/shelf_view_test_api.h"
#include "ash/shelf/shelf_widget.h"
#include "ash/test/ash_test_base.h"
#include "chromeos/constants/chromeos_switches.h"

namespace ash {

class TestShelfItemDelegate : public ShelfItemDelegate {
 public:
  explicit TestShelfItemDelegate(const ShelfID& shelf_id)
      : ShelfItemDelegate(shelf_id) {}

  // ShelfItemDelegate:
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

class ScrollableShelfViewTest : public AshTestBase {
 public:
  ScrollableShelfViewTest() = default;
  ~ScrollableShelfViewTest() override = default;

  void SetUp() override {
    base::CommandLine::ForCurrentProcess()->AppendSwitch(
        chromeos::switches::kShelfScrollable);
    AshTestBase::SetUp();
    scrollable_shelf_view_ = GetPrimaryShelf()
                                 ->shelf_widget()
                                 ->hotseat_widget()
                                 ->scrollable_shelf_view();
    shelf_view_ = scrollable_shelf_view_->shelf_view();
    test_api_ = std::make_unique<ShelfViewTestAPI>(
        scrollable_shelf_view_->shelf_view());
  }

 protected:
  ShelfID AddAppShortcut() {
    ShelfItem item = ShelfTestUtil::AddAppShortcut(base::NumberToString(id_++),
                                                   TYPE_PINNED_APP);

    // Wait for shelf view's bounds animation to end. Otherwise the scrollable
    // shelf's bounds are not updated yet.
    test_api_->RunMessageLoopUntilAnimationsDone();

    return item.id;
  }

  void AddAppShortcutUntilOverflow() {
    while (scrollable_shelf_view_->layout_strategy_for_test() ==
           ScrollableShelfView::kNotShowArrowButtons)
      AddAppShortcut();
  }

  ScrollableShelfView* scrollable_shelf_view_ = nullptr;
  ShelfView* shelf_view_ = nullptr;
  std::unique_ptr<ShelfViewTestAPI> test_api_;
  int id_ = 0;

 private:
  DISALLOW_COPY_AND_ASSIGN(ScrollableShelfViewTest);
};

// When hovering mouse on a shelf icon, the tooltip only shows for the visible
// icon (see https://crbug.com/997807).
TEST_F(ScrollableShelfViewTest, NotShowTooltipForHiddenIcons) {
  AddAppShortcutUntilOverflow();

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

}  // namespace ash
