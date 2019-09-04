// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_SHARING_SHARING_DIALOG_VIEW_H_
#define CHROME_BROWSER_UI_VIEWS_SHARING_SHARING_DIALOG_VIEW_H_

#include <memory>
#include <vector>

#include "chrome/browser/sharing/sharing_dialog.h"
#include "chrome/browser/sharing/sharing_ui_controller.h"
#include "chrome/browser/ui/views/location_bar/location_bar_bubble_delegate_view.h"
#include "ui/views/controls/button/button.h"
#include "ui/views/controls/styled_label_listener.h"

namespace views {
class StyledLabel;
class View;
}  // namespace views

class Browser;
class HoverButton;
enum class SharingDialogType;

class SharingDialogView : public SharingDialog,
                          public views::ButtonListener,
                          public views::StyledLabelListener,
                          public LocationBarBubbleDelegateView {
 public:
  // Bubble will be anchored to |anchor_view|.
  SharingDialogView(views::View* anchor_view,
                    content::WebContents* web_contents,
                    SharingUiController* controller);

  ~SharingDialogView() override;

  // SharingDialogView:
  void Hide() override;

  // views::WidgetDelegateView:
  bool ShouldShowCloseButton() const override;
  base::string16 GetWindowTitle() const override;
  void WindowClosing() override;

  // views::StyledLabelListener:
  void StyledLabelLinkClicked(views::StyledLabel* label,
                              const gfx::Range& range,
                              int event_flags) override;

  // views::DialogDelegate:
  int GetDialogButtons() const override;
  std::unique_ptr<views::View> CreateFootnoteView() override;

  // views::ButtonListener:
  void ButtonPressed(views::Button* sender, const ui::Event& event) override;

  // views::View:
  gfx::Size CalculatePreferredSize() const override;

  static views::BubbleDialogDelegateView* GetAsBubble(SharingDialog* dialog);

 private:
  friend class SharingDialogViewTest;
  FRIEND_TEST_ALL_PREFIXES(SharingDialogViewTest, PopulateDialogView);
  FRIEND_TEST_ALL_PREFIXES(SharingDialogViewTest, DevicePressed);
  FRIEND_TEST_ALL_PREFIXES(SharingDialogViewTest, AppPressed);

  SharingDialogType GetDialogType() const;

  // views::BubbleDialogDelegateView:
  void Init() override;

  // Populates the dialog view containing valid devices and apps.
  void InitListView();
  // Populates the dialog view containing no devices or apps.
  void InitEmptyView();
  // Populates the dialog view containing error help text.
  void InitErrorView();

  SharingUiController* controller_ = nullptr;
  // Contains references to device and app buttons in
  // the order they appear.
  std::vector<HoverButton*> dialog_buttons_;
  Browser* browser_ = nullptr;
  bool send_failed_ = false;

  DISALLOW_COPY_AND_ASSIGN(SharingDialogView);
};

#endif  // CHROME_BROWSER_UI_VIEWS_SHARING_SHARING_DIALOG_VIEW_H_
