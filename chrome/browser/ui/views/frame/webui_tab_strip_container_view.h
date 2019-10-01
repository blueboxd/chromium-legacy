// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_FRAME_WEBUI_TAB_STRIP_CONTAINER_VIEW_H_
#define CHROME_BROWSER_UI_VIEWS_FRAME_WEBUI_TAB_STRIP_CONTAINER_VIEW_H_

#include <memory>

#include "chrome/browser/ui/views/toolbar/toolbar_button.h"
#include "chrome/common/buildflags.h"
#include "ui/views/view.h"

#if !BUILDFLAG(ENABLE_WEBUI_TAB_STRIP)
#error
#endif

namespace views {
class WebView;
}  // namespace views

class Browser;

class WebUITabStripContainerView : public views::View,
                                   public views::ButtonListener {
 public:
  explicit WebUITabStripContainerView(Browser* browser);

  // Control buttons.
  std::unique_ptr<ToolbarButton> CreateNewTabButton();
  std::unique_ptr<ToolbarButton> CreateToggleButton();

 private:
  void ButtonPressed(views::Button* sender, const ui::Event& event) override;

  Browser* const browser_;
  views::WebView* const web_view_;
};

#endif  // CHROME_BROWSER_UI_VIEWS_FRAME_WEBUI_TAB_STRIP_CONTAINER_VIEW_H_
