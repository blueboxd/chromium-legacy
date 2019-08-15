// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_PROFILES_INCOGNITO_MENU_VIEW_H_
#define CHROME_BROWSER_UI_VIEWS_PROFILES_INCOGNITO_MENU_VIEW_H_

#include <stddef.h>

#include <memory>

#include "base/macros.h"
#include "chrome/browser/ui/views/profiles/profile_menu_view_base.h"

namespace views {
class Button;
}

class Browser;

// This bubble view is displayed when the user clicks on the avatar button in
// incognito mode and displays the incognito menu.
class IncognitoMenuView : public ProfileMenuViewBase {
 public:
  IncognitoMenuView(views::Button* anchor_button,
                    Browser* browser);
  ~IncognitoMenuView() override;

 private:
  // views::BubbleDialogDelegateView:
  void Init() override;
  base::string16 GetAccessibleWindowTitle() const override;

  void Reset();

  // Adds the incognito window count view.
  void AddIncognitoWindowCountView();

  // Button actions.
  void OnExitButtonClicked();

  views::Button* exit_button_;

  DISALLOW_COPY_AND_ASSIGN(IncognitoMenuView);
};

#endif  // CHROME_BROWSER_UI_VIEWS_PROFILES_INCOGNITO_MENU_VIEW_H_
