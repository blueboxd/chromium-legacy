// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_TOOLBAR_TAB_SEARCH_BUTTON_H_
#define CHROME_BROWSER_UI_VIEWS_TOOLBAR_TAB_SEARCH_BUTTON_H_

#include "chrome/browser/ui/views/toolbar/toolbar_button.h"
#include "ui/views/controls/button/button.h"

class Browser;

class TabSearchButton : public ToolbarButton, public views::ButtonListener {
 public:
  explicit TabSearchButton(Browser* browser);
  TabSearchButton(const TabSearchButton&) = delete;
  TabSearchButton& operator=(const TabSearchButton&) = delete;
  ~TabSearchButton() override = default;

  // ToolbarButton:
  void UpdateIcon() override;

  // views::ButtonListener:
  void ButtonPressed(views::Button* sender, const ui::Event& event) override;

 private:
  Browser* const browser_;
};

#endif  // CHROME_BROWSER_UI_VIEWS_TOOLBAR_TAB_SEARCH_BUTTON_H_
