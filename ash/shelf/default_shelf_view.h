// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SHELF_DEFAULT_SHELF_VIEW_H_
#define ASH_SHELF_DEFAULT_SHELF_VIEW_H_

#include <memory>

#include "ash/ash_export.h"
#include "ash/shelf/shelf_view.h"
#include "base/macros.h"

namespace views {
class View;
class Separator;
}  // namespace views

namespace ash {

class Shelf;
class ShelfModel;
class ShelfWidget;

// Default shelf view.
// TODO(agawrosnka): Move elements that are not common for default and Kiosk
// Next shelves from ShelfView to this subclass.
class ASH_EXPORT DefaultShelfView : public ShelfView {
 public:
  DefaultShelfView(ShelfModel* model, Shelf* shelf, ShelfWidget* shelf_widget);
  ~DefaultShelfView() override;

  void CalculateBackAndHomeButtonsIdealBounds();

  // All ShelfView overrides are public to keep them together.
  // ShelfView:
  void Init() override;
  void CalculateIdealBounds() override;
  void LayoutAppListAndBackButtonHighlight() override;
  std::unique_ptr<BackButton> CreateBackButton() override;
  std::unique_ptr<AppListButton> CreateHomeButton() override;

 private:
  struct AppCenteringStrategy {
    bool center_on_screen = false;
    bool overflow = false;
  };

  // Returns the size that's actually available for app icons. Size occupied
  // by the app list button and back button plus all appropriate margins is
  // not available for app icons.
  int GetAvailableSpaceForAppIcons() const;

  // Returns the index of the item after which the separator should be shown,
  // or -1 if no separator is required.
  int GetSeparatorIndex() const;

  // This method determines which centering strategy is adequate, returns that,
  // and sets the |first_visible_index_| and |last_visible_index_| fields
  // appropriately.
  AppCenteringStrategy CalculateAppCenteringStrategy();

  // Update all buttons' visibility in overflow.
  void UpdateAllButtonsVisibilityInOverflowMode();

  // A reference to the view used as a separator between pinned and unpinned
  // items.
  views::Separator* separator_ = nullptr;

  // A view to draw a background behind the app list and back buttons.
  // Owned by the view hierarchy.
  views::View* back_and_app_list_background_ = nullptr;

  DISALLOW_COPY_AND_ASSIGN(DefaultShelfView);
};

}  // namespace ash

#endif  // ASH_SHELF_DEFAULT_SHELF_VIEW_H_
