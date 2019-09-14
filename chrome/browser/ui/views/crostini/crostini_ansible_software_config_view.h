// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_CROSTINI_CROSTINI_ANSIBLE_SOFTWARE_CONFIG_VIEW_H_
#define CHROME_BROWSER_UI_VIEWS_CROSTINI_CROSTINI_ANSIBLE_SOFTWARE_CONFIG_VIEW_H_

#include "ui/views/bubble/bubble_dialog_delegate_view.h"

// The Ansible software configuration is shown to let the user know that an
// Ansible playbook is being applied and their app might take longer than
// usual to launch.
class CrostiniAnsibleSoftwareConfigView
    : public views::BubbleDialogDelegateView {
 public:
  // views::DialogDelegateView:
  int GetDialogButtons() const override;
  base::string16 GetWindowTitle() const override;
  bool ShouldShowCloseButton() const override;
  gfx::Size CalculatePreferredSize() const override;

  CrostiniAnsibleSoftwareConfigView();

 private:
  ~CrostiniAnsibleSoftwareConfigView() override;
};

#endif  // CHROME_BROWSER_UI_VIEWS_CROSTINI_CROSTINI_ANSIBLE_SOFTWARE_CONFIG_VIEW_H_
