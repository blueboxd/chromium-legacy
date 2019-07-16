// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_SHARING_CLICK_TO_CALL_CLICK_TO_CALL_DIALOG_VIEW_H_
#define CHROME_BROWSER_UI_VIEWS_SHARING_CLICK_TO_CALL_CLICK_TO_CALL_DIALOG_VIEW_H_

#include <memory>
#include <string>
#include <vector>

#include "chrome/browser/sharing/click_to_call/click_to_call_sharing_dialog_controller.h"
#include "chrome/browser/sharing/sharing_device_info.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/browser/ui/views/frame/toolbar_button_provider.h"
#include "chrome/browser/ui/views/hover_button.h"
#include "chrome/browser/ui/views/location_bar/location_bar_bubble_delegate_view.h"
#include "chrome/browser/ui/views/page_action/omnibox_page_action_icon_container_view.h"
#include "ui/views/controls/button/button.h"

namespace views {
class View;
}  // namespace views

class PageActionIconView;

class ClickToCallDialogView : public views::ButtonListener,
                              public LocationBarBubbleDelegateView {
 public:
  // Bubble will be anchored to |anchor_view|.
  ClickToCallDialogView(
      views::View* anchor_view,
      PageActionIconView* icon_view,
      content::WebContents* web_contents,
      std::unique_ptr<ClickToCallSharingDialogController> controller);

  ~ClickToCallDialogView() override;

  // views::WidgetDelegateView:
  bool ShouldShowCloseButton() const override;
  base::string16 GetWindowTitle() const override;

  // views::DialogDelegate:
  int GetDialogButtons() const override;

  // views::ButtonListener:
  void ButtonPressed(views::Button* sender, const ui::Event& event) override;

  // views::View:
  gfx::Size CalculatePreferredSize() const override;

 private:
  friend class ClickToCallDialogViewTest;
  FRIEND_TEST_ALL_PREFIXES(ClickToCallDialogViewTest, PopulateDialogView);
  FRIEND_TEST_ALL_PREFIXES(ClickToCallDialogViewTest, DevicePressed);
  FRIEND_TEST_ALL_PREFIXES(ClickToCallDialogViewTest, AppPressed);

  // views::BubbleDialogDelegateView:
  void Init() override;

  // Populates the dialog view containing valid devices and apps.
  void PopulateDialogView();

  PageActionIconView* icon_view_;
  std::unique_ptr<ClickToCallSharingDialogController> controller_;
  // Contains references to device and app buttons in
  // the order they appear.
  std::vector<HoverButton*> dialog_buttons_;
  std::vector<SharingDeviceInfo> devices_;
  std::vector<ClickToCallSharingDialogController::App> apps_;

  DISALLOW_COPY_AND_ASSIGN(ClickToCallDialogView);
};

#endif  // CHROME_BROWSER_UI_VIEWS_SHARING_CLICK_TO_CALL_CLICK_TO_CALL_DIALOG_VIEW_H_
