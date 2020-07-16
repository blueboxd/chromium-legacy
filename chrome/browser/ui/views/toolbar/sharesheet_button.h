// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_TOOLBAR_SHARESHEET_BUTTON_H_
#define CHROME_BROWSER_UI_VIEWS_TOOLBAR_SHARESHEET_BUTTON_H_

#include "chrome/browser/ui/views/toolbar/toolbar_button.h"

class Browser;

class SharesheetButton : public ToolbarButton, public views::ButtonListener {
 public:
  explicit SharesheetButton(Browser* browser);
  SharesheetButton(const SharesheetButton&) = delete;
  SharesheetButton& operator=(const SharesheetButton&) = delete;
  ~SharesheetButton() override;

  // ToolbarButton:
  void UpdateIcon() override;

 private:
  // views::ButtonListener:
  void ButtonPressed(views::Button* sender, const ui::Event& event) override;

  int GetIconSize() const;

  Browser* const browser_;
};

#endif  // CHROME_BROWSER_UI_VIEWS_TOOLBAR_SHARESHEET_BUTTON_H_
