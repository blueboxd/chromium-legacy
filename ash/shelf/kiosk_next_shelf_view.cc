// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/shelf/kiosk_next_shelf_view.h"

#include <memory>

#include "ash/display/screen_orientation_controller.h"
#include "ash/home_screen/home_screen_controller.h"
#include "ash/public/cpp/shelf_model.h"
#include "ash/shelf/app_list_button.h"
#include "ash/shelf/back_button.h"
#include "ash/shelf/overflow_button.h"
#include "ash/shelf/shelf.h"
#include "ash/shelf/shelf_constants.h"
#include "ash/shelf/shelf_widget.h"
#include "ash/shell.h"
#include "ash/system/status_area_widget.h"
#include "ash/wm/tablet_mode/tablet_mode_controller.h"
#include "base/logging.h"
#include "ui/gfx/canvas.h"
#include "ui/gfx/geometry/insets.h"
#include "ui/gfx/geometry/insets_f.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/geometry/size.h"
#include "ui/views/animation/flood_fill_ink_drop_ripple.h"
#include "ui/views/animation/ink_drop_mask.h"
#include "ui/views/animation/ink_drop_ripple.h"
#include "ui/views/view.h"
#include "ui/views/widget/widget.h"

namespace ash {

namespace {

// Kiosk Next back button with permanent round rectangle background.
class KioskNextBackButton : public BackButton {
 public:
  explicit KioskNextBackButton(ShelfView* shelf_view)
      : BackButton(shelf_view) {}
  ~KioskNextBackButton() override = default;

 private:
  // views::BackButton:
  void PaintButtonContents(gfx::Canvas* canvas) override {
    PaintBackground(canvas, GetContentsBounds());
    BackButton::PaintButtonContents(canvas);
  }

  std::unique_ptr<views::InkDropMask> CreateInkDropMask() const override {
    return std::make_unique<views::RoundRectInkDropMask>(
        size(), gfx::InsetsF(), kKioskNextShelfControlWidthDp / 2);
  }

  std::unique_ptr<views::InkDropRipple> CreateInkDropRipple() const override {
    return std::make_unique<views::FloodFillInkDropRipple>(
        size(), gfx::Insets(), GetInkDropCenterBasedOnLastEvent(),
        GetInkDropBaseColor(), ink_drop_visible_opacity());
  }

  DISALLOW_COPY_AND_ASSIGN(KioskNextBackButton);
};

// Kiosk Next home button with permanent round rectangle background.
class KioskNextHomeButton : public AppListButton {
 public:
  KioskNextHomeButton(ShelfView* shelf_view, Shelf* shelf)
      : AppListButton(shelf_view, shelf) {}
  ~KioskNextHomeButton() override = default;

 private:
  // views::AppListButton:
  void PaintButtonContents(gfx::Canvas* canvas) override {
    PaintBackground(canvas, GetContentsBounds());
    AppListButton::PaintButtonContents(canvas);
  }

  std::unique_ptr<views::InkDropMask> CreateInkDropMask() const override {
    return std::make_unique<views::RoundRectInkDropMask>(
        size(), gfx::InsetsF(), kKioskNextShelfControlWidthDp / 2);
  }

  std::unique_ptr<views::InkDropRipple> CreateInkDropRipple() const override {
    return std::make_unique<views::FloodFillInkDropRipple>(
        size(), gfx::Insets(), GetInkDropCenterBasedOnLastEvent(),
        GetInkDropBaseColor(), ink_drop_visible_opacity());
  }

  void OnPressed(app_list::AppListShowSource show_source,
                 base::TimeTicks time_stamp) override {
    Shell::Get()->home_screen_controller()->GoHome(
        shelf_view()->GetDisplayId());
  }

  DISALLOW_COPY_AND_ASSIGN(KioskNextHomeButton);
};

bool IsTabletModeEnabled() {
  // This check is needed, because tablet mode controller is destroyed before
  // shelf widget. See https://crbug.com/967149 for more details.
  return Shell::Get()->tablet_mode_controller() &&
         Shell::Get()->tablet_mode_controller()->InTabletMode();
}

}  // namespace

KioskNextShelfView::KioskNextShelfView(ShelfModel* model,
                                       Shelf* shelf,
                                       ShelfWidget* shelf_widget)
    : ShelfView(model, shelf, shelf_widget) {
  // Kiosk next shelf has 2 buttons and it is expected to be used in tablet mode
  // with bottom shelf alignment. It can be adapted to different requirements,
  // but they should be explicitly verified first.
  DCHECK(IsTabletModeEnabled());
  DCHECK(shelf->IsHorizontalAlignment());
  DCHECK_EQ(2, model->item_count());
}

KioskNextShelfView::~KioskNextShelfView() = default;

void KioskNextShelfView::Init() {
  ShelfView::Init();

  // Needs to be called after base class call that creates below objects.
  // TODO(agawronska): Separator and overflow button are not needed in Kiosk
  // Next shelf. They should be moved to DefaultShelfView subclass and the below
  // code should be removed.
  DCHECK(overflow_button());
  overflow_button()->SetVisible(false);

  set_first_visible_index(0);
  set_last_visible_index(model()->item_count() - 1);
}

void KioskNextShelfView::CalculateIdealBounds() {
  DCHECK(shelf()->IsHorizontalAlignment());
  DCHECK_EQ(2, model()->item_count());
  DCHECK_GE(kShelfSize, kKioskNextShelfControlHeightDp);

  // TODO(https://crbug.com/965690): Button spacing might be relative to shelf
  // width. Reevaluate this piece once visual spec is available.
  const int control_buttons_spacing =
      IsCurrentScreenOrientationLandscape()
          ? kKioskNextShelfControlSpacingLandscapeDp
          : kKioskNextShelfControlSpacingPortraitDp;
  const int total_shelf_width =
      shelf_widget()->GetWindowBoundsInScreen().width();
  int x = total_shelf_width / 2 - kKioskNextShelfControlWidthDp -
          control_buttons_spacing / 2;
  int y = (kShelfSize - kKioskNextShelfControlHeightDp) / 2;

  GetBackButton()->set_ideal_bounds(gfx::Rect(
      x, y, kKioskNextShelfControlWidthDp, kKioskNextShelfControlHeightDp));
  x += (kKioskNextShelfControlWidthDp + control_buttons_spacing);
  GetAppListButton()->set_ideal_bounds(gfx::Rect(
      x, y, kKioskNextShelfControlWidthDp, kKioskNextShelfControlHeightDp));
}

std::unique_ptr<BackButton> KioskNextShelfView::CreateBackButton() {
  return std::make_unique<KioskNextBackButton>(this);
}

std::unique_ptr<AppListButton> KioskNextShelfView::CreateHomeButton() {
  return std::make_unique<KioskNextHomeButton>(this, shelf());
}

}  // namespace ash
