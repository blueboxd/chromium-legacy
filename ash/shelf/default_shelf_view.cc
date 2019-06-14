// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/shelf/default_shelf_view.h"

#include "ash/public/cpp/shelf_item.h"
#include "ash/public/cpp/shelf_model.h"
#include "ash/shelf/app_list_button.h"
#include "ash/shelf/back_button.h"
#include "ash/shelf/overflow_button.h"
#include "ash/shelf/shelf.h"
#include "ash/shelf/shelf_app_button.h"
#include "ash/shelf/shelf_constants.h"
#include "ash/shelf/shelf_widget.h"
#include "ash/shell.h"
#include "ash/system/model/system_tray_model.h"
#include "ash/system/model/virtual_keyboard_model.h"
#include "ash/system/status_area_widget.h"
#include "ash/wm/tablet_mode/tablet_mode_controller.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/views/controls/separator.h"
#include "ui/views/view.h"
#include "ui/views/view_model.h"

namespace ash {

namespace {

// Indices of the start-aligned system buttons (the "back" button in tablet
// mode, and the "app list" button).
constexpr int kBackButtonIndex = 0;
constexpr int kAppListButtonIndex = 1;

// White with ~20% opacity.
constexpr SkColor kSeparatorColor = SkColorSetARGB(0x32, 0xFF, 0xFF, 0xFF);

// The dimensions, in pixels, of the separator between pinned and unpinned
// items.
constexpr int kSeparatorSize = 20;
constexpr int kSeparatorThickness = 1;

// The margin between app icons and the overflow button. This is bigger than
// between app icons because the overflow button is a little smaller.
constexpr int kMarginBetweenAppsAndOverflow = 16;

// The margin on either side of the group of app icons (including the overflow
// button).
constexpr int kAppIconGroupMargin = 16;

// Helper to check if tablet mode is enabled.
bool IsTabletModeEnabled() {
  // This check is needed, because tablet mode controller is destroyed before
  // shelf widget. See https://crbug.com/967149 for more details.
  return Shell::Get()->tablet_mode_controller() &&
         Shell::Get()
             ->tablet_mode_controller()
             ->IsTabletModeWindowManagerEnabled();
}

// Returns the size occupied by |count| app icons. If |with_overflow| is
// true, returns the size of |count| app icons followed by an overflow
// button.
int GetSizeOfAppIcons(int count, bool with_overflow) {
  if (count == 0)
    return with_overflow ? kShelfControlSize : 0;

  const int app_size = count * kShelfButtonSize;
  int overflow_size = 0;
  int total_padding = ShelfConstants::button_spacing() * (count - 1);
  if (with_overflow) {
    overflow_size += kShelfControlSize;
    total_padding += kMarginBetweenAppsAndOverflow;
  }
  return app_size + total_padding + overflow_size;
}

}  // namespace

DefaultShelfView::DefaultShelfView(ShelfModel* model,
                                   Shelf* shelf,
                                   ShelfWidget* shelf_widget)
    : ShelfView(model, shelf, shelf_widget) {}

DefaultShelfView::~DefaultShelfView() = default;

void DefaultShelfView::CalculateBackAndHomeButtonsIdealBounds() {
  if (is_overflow_mode())
    return;

  const int button_spacing = ShelfConstants::button_spacing();
  int x = shelf()->PrimaryAxisValue(button_spacing, 0);
  int y = shelf()->PrimaryAxisValue(0, button_spacing);

  GetBackButton()->set_ideal_bounds(gfx::Rect(
      x, y, shelf()->PrimaryAxisValue(kShelfControlSize, kShelfButtonSize),
      shelf()->PrimaryAxisValue(kShelfButtonSize, kShelfControlSize)));

  // If we are not in tablet mode, the home button will get placed on top of
  // the (invisible) back button.
  if (IsTabletModeEnabled()) {
    x = shelf()->PrimaryAxisValue(x + kShelfControlSize + button_spacing, x);
    y = shelf()->PrimaryAxisValue(y, y + kShelfControlSize + button_spacing);
  }

  GetAppListButton()->set_ideal_bounds(gfx::Rect(
      x, y, shelf()->PrimaryAxisValue(kShelfControlSize, kShelfButtonSize),
      shelf()->PrimaryAxisValue(kShelfButtonSize, kShelfControlSize)));
}

void DefaultShelfView::Init() {
  // Add the background view behind the app list and back buttons first, so
  // that other views will appear above it.
  back_and_app_list_background_ = new views::View();
  back_and_app_list_background_->set_can_process_events_within_subtree(false);
  back_and_app_list_background_->SetBackground(
      CreateBackgroundFromPainter(views::Painter::CreateSolidRoundRectPainter(
          kShelfControlPermanentHighlightBackground,
          ShelfConstants::control_border_radius())));
  ConfigureChildView(back_and_app_list_background_);
  AddChildView(back_and_app_list_background_);

  separator_ = new views::Separator();
  separator_->SetColor(kSeparatorColor);
  separator_->SetPreferredHeight(kSeparatorSize);
  separator_->SetVisible(false);
  ConfigureChildView(separator_);
  AddChildView(separator_);

  // Calling Init() after adding background view, so control buttons are added
  // after background.
  ShelfView::Init();
}

void DefaultShelfView::CalculateIdealBounds() {
  DCHECK(model()->item_count() == view_model()->view_size());

  const int button_spacing = ShelfConstants::button_spacing();
  const int available_size = shelf()->PrimaryAxisValue(width(), height());

  const int separator_index = GetSeparatorIndex();
  const AppCenteringStrategy app_centering_strategy =
      CalculateAppCenteringStrategy();

  // At this point we know that |last_visible_index_| is up to date.
  const bool virtual_keyboard_visible =
      Shell::Get()->system_tray_model()->virtual_keyboard()->visible();
  // Don't show the separator if it isn't needed, or would appear after all
  // visible items.
  separator_->SetVisible(separator_index != -1 &&
                         separator_index < last_visible_index() &&
                         !virtual_keyboard_visible);

  int x = 0;
  int y = 0;
  if (!is_overflow_mode()) {
    // Calculate the bounds for the back and home buttons separately.
    CalculateBackAndHomeButtonsIdealBounds();

    int app_list_button_position =
        shelf()->PrimaryAxisValue(GetAppListButton()->ideal_bounds().right(),
                                  GetAppListButton()->ideal_bounds().bottom());

    x = shelf()->PrimaryAxisValue(app_list_button_position, 0);
    y = shelf()->PrimaryAxisValue(0, app_list_button_position);

    // A larger minimum padding after the app list button is required:
    // increment with the necessary extra amount.
    x += shelf()->PrimaryAxisValue(kAppIconGroupMargin - button_spacing, 0);
    y += shelf()->PrimaryAxisValue(0, kAppIconGroupMargin - button_spacing);

    // Now add the necessary padding to center app icons.
    StatusAreaWidget* status_widget = shelf_widget()->status_area_widget();
    const int status_widget_size =
        status_widget ? shelf()->PrimaryAxisValue(
                            status_widget->GetWindowBoundsInScreen().width(),
                            status_widget->GetWindowBoundsInScreen().height())
                      : 0;
    const int screen_size = available_size + status_widget_size;

    const int available_size_for_app_icons = GetAvailableSpaceForAppIcons();
    const int icons_size = GetSizeOfAppIcons(number_of_visible_apps(),
                                             app_centering_strategy.overflow);
    int padding_for_centering = 0;

    if (app_centering_strategy.center_on_screen) {
      padding_for_centering = (screen_size - icons_size) / 2;
    } else {
      padding_for_centering =
          kShelfButtonSpacing +
          (IsTabletModeEnabled() ? 2 : 1) * kShelfControlSize +
          kAppIconGroupMargin + (available_size_for_app_icons - icons_size) / 2;
    }

    if (padding_for_centering >
        app_list_button_position + kAppIconGroupMargin) {
      // Only shift buttons to the right, never let them interfere with the
      // left-aligned system buttons.
      x = shelf()->PrimaryAxisValue(padding_for_centering, 0);
      y = shelf()->PrimaryAxisValue(0, padding_for_centering);
    }
  }

  // We've already processed the back and home buttons.
  for (int i = kAppListButtonIndex + 1; i < view_model()->view_size(); ++i) {
    if (is_overflow_mode() && i < first_visible_index()) {
      view_model()->set_ideal_bounds(i, gfx::Rect(x, y, 0, 0));
      continue;
    }

    view_model()->set_ideal_bounds(
        i,
        gfx::Rect(
            x, y, shelf()->PrimaryAxisValue(kShelfButtonSize, kShelfButtonSize),
            shelf()->PrimaryAxisValue(kShelfButtonSize, kShelfButtonSize)));

    x = shelf()->PrimaryAxisValue(x + kShelfButtonSize + button_spacing, x);
    y = shelf()->PrimaryAxisValue(y, y + kShelfButtonSize + button_spacing);

    if (i == separator_index) {
      // Place the separator halfway between the two icons it separates,
      // vertically centered.
      int half_space = button_spacing / 2;
      int secondary_offset =
          (ShelfConstants::shelf_size() - kSeparatorSize) / 2;
      x -= shelf()->PrimaryAxisValue(half_space, 0);
      y -= shelf()->PrimaryAxisValue(0, half_space);
      separator_->SetBounds(
          x + shelf()->PrimaryAxisValue(0, secondary_offset),
          y + shelf()->PrimaryAxisValue(secondary_offset, 0),
          shelf()->PrimaryAxisValue(kSeparatorThickness, kSeparatorSize),
          shelf()->PrimaryAxisValue(kSeparatorSize, kSeparatorThickness));
      x += shelf()->PrimaryAxisValue(half_space, 0);
      y += shelf()->PrimaryAxisValue(0, half_space);
    }
  }

  if (is_overflow_mode()) {
    UpdateAllButtonsVisibilityInOverflowMode();
    return;
  }
  // In the main shelf, the first visible index is either the back button (in
  // tablet mode) or the launcher button (otherwise).
  if (!is_overflow_mode()) {
    set_first_visible_index(IsTabletModeEnabled() ? kBackButtonIndex
                                                  : kAppListButtonIndex);
  }

  for (int i = 0; i < view_model()->view_size(); ++i) {
    // To receive drag event continuously from |drag_view_| during the dragging
    // off from the shelf, don't make |drag_view_| invisible. It will be
    // eventually invisible and removed from the |view_model_| by
    // FinalizeRipOffDrag().
    if (dragged_off_shelf() && view_model()->view_at(i) == drag_view())
      continue;
    // If the virtual keyboard is visible, only the back button and the app
    // list button are shown.
    const bool is_visible_item = !virtual_keyboard_visible ||
                                 i == kBackButtonIndex ||
                                 i == kAppListButtonIndex;
    view_model()->view_at(i)->SetVisible(i <= last_visible_index() &&
                                         is_visible_item);
  }

  overflow_button()->SetVisible(app_centering_strategy.overflow);
  if (app_centering_strategy.overflow) {
    if (overflow_bubble() && overflow_bubble()->IsShowing())
      UpdateOverflowRange(overflow_bubble()->bubble_view()->shelf_view());
  } else {
    if (overflow_bubble())
      overflow_bubble()->Hide();
  }
}

void DefaultShelfView::LayoutAppListAndBackButtonHighlight() {
  // Don't show anything if this is the overflow menu.
  if (is_overflow_mode()) {
    back_and_app_list_background_->SetVisible(false);
    return;
  }
  const int button_spacing = ShelfConstants::button_spacing();
  // "Secondary" as in "orthogonal to the shelf primary axis".
  const int control_secondary_padding =
      (ShelfConstants::shelf_size() - kShelfControlSize) / 2;
  const int back_and_app_list_background_size =
      kShelfControlSize +
      (IsTabletModeEnabled() ? kShelfControlSize + button_spacing : 0);
  back_and_app_list_background_->SetBounds(
      shelf()->PrimaryAxisValue(button_spacing, control_secondary_padding),
      shelf()->PrimaryAxisValue(control_secondary_padding, button_spacing),
      shelf()->PrimaryAxisValue(back_and_app_list_background_size,
                                kShelfControlSize),
      shelf()->PrimaryAxisValue(kShelfControlSize,
                                back_and_app_list_background_size));
}

std::unique_ptr<BackButton> DefaultShelfView::CreateBackButton() {
  return std::make_unique<BackButton>(this);
}

std::unique_ptr<AppListButton> DefaultShelfView::CreateHomeButton() {
  return std::make_unique<AppListButton>(this, shelf());
}

int DefaultShelfView::GetAvailableSpaceForAppIcons() const {
  // Subtract space already allocated to the app list button, and the back
  // button if applicable.
  return shelf()->PrimaryAxisValue(width(), height()) - kShelfButtonSpacing -
         (IsTabletModeEnabled() ? 2 : 1) * kShelfControlSize -
         2 * kAppIconGroupMargin;
}

int DefaultShelfView::GetSeparatorIndex() const {
  for (int i = 0; i < model()->item_count() - 1; ++i) {
    if (IsItemPinned(model()->items()[i]) &&
        model()->items()[i + 1].type == TYPE_APP) {
      return i;
    }
  }
  return -1;
}

DefaultShelfView::AppCenteringStrategy
DefaultShelfView::CalculateAppCenteringStrategy() {
  // There are two possibilities. Either all the apps fit when centered
  // on the whole screen width, in which case we do that. Or, when space
  // becomes a little tight (which happens especially when the status area
  // is wider because of extra panels), we center apps on the available space.

  AppCenteringStrategy strategy;
  // This is only relevant for the main shelf.
  if (is_overflow_mode())
    return strategy;

  const int total_available_size = shelf()->PrimaryAxisValue(width(), height());
  StatusAreaWidget* status_widget = shelf_widget()->status_area_widget();
  const int status_widget_size =
      status_widget ? shelf()->PrimaryAxisValue(
                          status_widget->GetWindowBoundsInScreen().width(),
                          status_widget->GetWindowBoundsInScreen().height())
                    : 0;
  const int screen_size = total_available_size + status_widget_size;

  // An easy way to check whether the apps fit at the exact center of the
  // screen is to imagine that we have another status widget on the other
  // side (the status widget is always bigger than the app list button plus
  // the back button if applicable) and see if the apps can fit in the middle.
  int available_space_for_screen_centering =
      screen_size - 2 * (status_widget_size + kAppIconGroupMargin);

  // Views at index 0 and 1 are the back button and app list button.
  if (GetSizeOfAppIcons(view_model()->view_size() - 2, false) <
      available_space_for_screen_centering) {
    // Everything fits in the center of the screen.
    set_last_visible_index(view_model()->view_size() - 1);
    strategy.center_on_screen = true;
    return strategy;
  }

  const int available_size_for_app_icons = GetAvailableSpaceForAppIcons();
  set_last_visible_index(1);
  // We know that replacing the last app that fits with the overflow button
  // will not change the outcome, so we ignore that case for now.
  while (last_visible_index() < view_model()->view_size() - 1) {
    if (GetSizeOfAppIcons(last_visible_index(), false) <=
        available_size_for_app_icons) {
      set_last_visible_index(last_visible_index() + 1);
    } else {
      strategy.overflow = true;
      // Make space for the overflow button by showing one fewer app icon.
      set_last_visible_index(last_visible_index() - 1);
      break;
    }
  }
  return strategy;
}

void DefaultShelfView::UpdateAllButtonsVisibilityInOverflowMode() {
  // The overflow button is not shown in overflow mode.
  overflow_button()->SetVisible(false);
  DCHECK_LT(last_visible_index(), view_model()->view_size());
  for (int i = 0; i < view_model()->view_size(); ++i) {
    bool visible = i >= first_visible_index() && i <= last_visible_index();
    // To track the dragging of |drag_view_| continuously, its visibility
    // should be always true regardless of its position.
    if (dragged_to_another_shelf() && view_model()->view_at(i) == drag_view())
      view_model()->view_at(i)->SetVisible(true);
    else
      view_model()->view_at(i)->SetVisible(visible);
  }
}

}  // namespace ash
