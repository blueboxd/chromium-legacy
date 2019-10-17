// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_FRAME_WEBUI_TAB_STRIP_CONTAINER_VIEW_H_
#define CHROME_BROWSER_UI_VIEWS_FRAME_WEBUI_TAB_STRIP_CONTAINER_VIEW_H_

#include <memory>

#include "chrome/browser/ui/views/toolbar/toolbar_button.h"
#include "chrome/browser/ui/webui/tab_strip/tab_strip_ui.h"
#include "chrome/common/buildflags.h"
#include "ui/views/view.h"

#if !BUILDFLAG(ENABLE_WEBUI_TAB_STRIP)
#error
#endif

namespace ui {
class MenuModel;
}  // namespace ui

namespace views {
class MenuRunner;
class NativeViewHost;
class WebView;
}  // namespace views

class Browser;

class WebUITabStripContainerView : public TabStripUI::Embedder,
                                   public views::View,
                                   public views::ButtonListener {
 public:
  explicit WebUITabStripContainerView(Browser* browser);
  ~WebUITabStripContainerView() override;

  views::NativeViewHost* GetNativeViewHost();

  // Control buttons.
  std::unique_ptr<ToolbarButton> CreateNewTabButton();
  std::unique_ptr<ToolbarButton> CreateToggleButton();

 private:
  // TabStripUI::Embedder:
  void CloseContainer() override;
  void ShowContextMenuAtPoint(
      gfx::Point point,
      std::unique_ptr<ui::MenuModel> menu_model) override;

  // views::View:
  int GetHeightForWidth(int w) const override;

  // views::ButtonListener:
  void ButtonPressed(views::Button* sender, const ui::Event& event) override;

  Browser* const browser_;
  views::WebView* const web_view_;

  std::unique_ptr<views::MenuRunner> context_menu_runner_;
  std::unique_ptr<ui::MenuModel> context_menu_model_;
};

#endif  // CHROME_BROWSER_UI_VIEWS_FRAME_WEBUI_TAB_STRIP_CONTAINER_VIEW_H_
