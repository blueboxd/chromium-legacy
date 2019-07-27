// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_APP_LIST_VIEWS_PAGE_SWITCHER_H_
#define ASH_APP_LIST_VIEWS_PAGE_SWITCHER_H_

#include "ash/public/cpp/pagination/pagination_model_observer.h"
#include "base/macros.h"
#include "ui/views/controls/button/button.h"

namespace ash {
class PaginationModel;
}

namespace app_list {

// PageSwitcher represents its underlying PaginationModel with a button
// strip. Each page in the PageinationModel has a button in the strip and
// when the button is clicked, the corresponding page becomes selected.
class PageSwitcher : public views::View,
                     public views::ButtonListener,
                     public ash::PaginationModelObserver {
 public:
  PageSwitcher(ash::PaginationModel* model, bool vertical, bool is_tablet_mode);
  ~PageSwitcher() override;

  // Overridden from views::View:
  gfx::Size CalculatePreferredSize() const override;
  void Layout() override;
  const char* GetClassName() const override;

  void set_ignore_button_press(bool ignore) { ignore_button_press_ = ignore; }
  void set_is_tablet_mode(bool started) { is_tablet_mode_ = started; }

 private:
  // Overridden from views::ButtonListener:
  void ButtonPressed(views::Button* sender, const ui::Event& event) override;

  // Overridden from PaginationModelObserver:
  void TotalPagesChanged() override;
  void SelectedPageChanged(int old_selected, int new_selected) override;

  ash::PaginationModel* model_;  // Owned by AppsGridView.
  views::View* buttons_;    // Owned by views hierarchy.

  // True if the page switcher button strip should grow vertically.
  const bool vertical_;

  // True if button press should be ignored.
  bool ignore_button_press_ = false;

  // Whether tablet mode is enabled.
  bool is_tablet_mode_;

  DISALLOW_COPY_AND_ASSIGN(PageSwitcher);
};

}  // namespace app_list

#endif  // ASH_APP_LIST_VIEWS_PAGE_SWITCHER_H_
