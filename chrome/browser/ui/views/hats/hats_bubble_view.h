// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_HATS_HATS_BUBBLE_VIEW_H_
#define CHROME_BROWSER_UI_VIEWS_HATS_HATS_BUBBLE_VIEW_H_

#include <stddef.h>

#include <string>

#include "base/macros.h"
#include "chrome/browser/ui/views/close_bubble_on_tab_activation_helper.h"
#include "ui/views/bubble/bubble_dialog_delegate_view.h"

class AppMenuButton;
class Browser;

// This bubble view is displayed when a Happiness tracking survey is triggered.
// It displays a WebUI that hosts the survey.
class HatsBubbleView : public views::BubbleDialogDelegateView {
 public:
  // Returns a pointer to the Hats Bubble being shown. For testing only.
  static views::BubbleDialogDelegateView* GetHatsBubble();
  // Creates and shows the bubble that leads to a survey identified by
  // |site_id|.
  static void Show(Browser* browser, const std::string& site_id);

 protected:
  // views::WidgetDelegate:
  base::string16 GetWindowTitle() const override;
  bool ShouldShowCloseButton() const override;
  gfx::ImageSkia GetWindowIcon() override;
  bool ShouldShowWindowIcon() const override;

  // views::DialogDelegate:
  base::string16 GetDialogButtonLabel(ui::DialogButton button) const override;
  bool Accept() override;

  // views::WidgetObserver:
  void OnWidgetDestroying(views::Widget* widget) override;

  // views::View:
  void Layout() override;

 private:
  HatsBubbleView(Browser* browser,
                 AppMenuButton* anchor_button,
                 const std::string& site_id,
                 gfx::NativeView parent_view);
  ~HatsBubbleView() override;

  static HatsBubbleView* instance_;
  CloseBubbleOnTabActivationHelper close_bubble_helper_;
  const std::string site_id_;
  const Browser* browser_;

  DISALLOW_COPY_AND_ASSIGN(HatsBubbleView);
};

#endif  // CHROME_BROWSER_UI_VIEWS_HATS_HATS_BUBBLE_VIEW_H_
