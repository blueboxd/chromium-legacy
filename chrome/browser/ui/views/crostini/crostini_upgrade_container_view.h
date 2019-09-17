// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_CROSTINI_CROSTINI_UPGRADE_CONTAINER_VIEW_H_
#define CHROME_BROWSER_UI_VIEWS_CROSTINI_CROSTINI_UPGRADE_CONTAINER_VIEW_H_

#include "ui/views/bubble/bubble_dialog_delegate_view.h"

class Profile;

namespace crostini {
void SetCrostiniUpgradeSkipDelayForTesting(bool should_skip);
}  // namespace crostini

// Provides a warning to the user that an upgrade is occurring and Crostini
// start will take longer than usual.
class CrostiniUpgradeContainerView : public views::BubbleDialogDelegateView {
 public:
  static void Show(Profile* profile);

  // views::DialogDelegateView:
  int GetDialogButtons() const override;
  base::string16 GetWindowTitle() const override;
  bool ShouldShowCloseButton() const override;
  gfx::Size CalculatePreferredSize() const override;

  static CrostiniUpgradeContainerView* GetActiveViewForTesting();

 private:
  CrostiniUpgradeContainerView();
  ~CrostiniUpgradeContainerView() override;
};

#endif  // CHROME_BROWSER_UI_VIEWS_CROSTINI_CROSTINI_UPGRADE_CONTAINER_VIEW_H_
