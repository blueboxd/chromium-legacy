// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_TOOLBAR_BACK_FORWARD_BUTTON_H_
#define CHROME_BROWSER_UI_VIEWS_TOOLBAR_BACK_FORWARD_BUTTON_H_

#include "chrome/browser/ui/views/toolbar/toolbar_button.h"

class Browser;

class BackForwardButton : public ToolbarButton {
 public:
  enum class Direction { kBack, kForward };

  BackForwardButton(Direction direction,
                    views::ButtonListener* listener,
                    Browser* browser);
  BackForwardButton(const BackForwardButton&) = delete;
  BackForwardButton& operator=(const BackForwardButton&) = delete;
  ~BackForwardButton() override;

  // TODO(http://crbug.com/1099607) Remove this once WebAppFrameToolbarView
  // doesn't need this.
  void OverrideImageModel(const ui::ImageModel& normal,
                          const ui::ImageModel& disabled);

  // ToolbarButton:
  void UpdateIcon() override;

 private:
  Direction direction_;

  base::Optional<ui::ImageModel> normal_model_;
  base::Optional<ui::ImageModel> disabled_model_;
};

#endif  // CHROME_BROWSER_UI_VIEWS_TOOLBAR_BACK_FORWARD_BUTTON_H_
