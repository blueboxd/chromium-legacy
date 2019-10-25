// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_FRAME_WEBUI_TAB_STRIP_CONTAINER_VIEW_H_
#define CHROME_BROWSER_UI_VIEWS_FRAME_WEBUI_TAB_STRIP_CONTAINER_VIEW_H_

#include <memory>

#include "base/scoped_observer.h"
#include "chrome/browser/ui/views/toolbar/toolbar_button.h"
#include "chrome/browser/ui/webui/tab_strip/tab_strip_ui.h"
#include "chrome/common/buildflags.h"
#include "ui/events/event_handler.h"
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
                                   public views::ButtonListener,
                                   public views::ViewObserver {
 public:
  WebUITabStripContainerView(Browser* browser,
                             views::View* tab_contents_container);
  ~WebUITabStripContainerView() override;

  views::NativeViewHost* GetNativeViewHost();

  // Control buttons.
  std::unique_ptr<ToolbarButton> CreateNewTabButton();
  std::unique_ptr<ToolbarButton> CreateToggleButton();

 private:
  class AutoCloser;

  // TabStripUI::Embedder:
  void CloseContainer() override;
  void ShowContextMenuAtPoint(
      gfx::Point point,
      std::unique_ptr<ui::MenuModel> menu_model) override;
  TabStripUILayout GetLayout() override;

  // views::View:
  int GetHeightForWidth(int w) const override;

  // views::ButtonListener:
  void ButtonPressed(views::Button* sender, const ui::Event& event) override;

  // views::ViewObserver:
  void OnViewBoundsChanged(View* observed_view) override;

  Browser* const browser_;
  views::WebView* const web_view_;
  views::View* const tab_contents_container_;

  int desired_height_ = 0;

  std::unique_ptr<AutoCloser> auto_closer_;

  std::unique_ptr<views::MenuRunner> context_menu_runner_;
  std::unique_ptr<ui::MenuModel> context_menu_model_;

  ScopedObserver<views::View, views::ViewObserver> view_observer_{this};
};

#endif  // CHROME_BROWSER_UI_VIEWS_FRAME_WEBUI_TAB_STRIP_CONTAINER_VIEW_H_
