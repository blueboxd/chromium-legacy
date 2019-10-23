// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/crostini/crostini_ansible_software_config_view.h"

#include "base/bind_helpers.h"
#include "chrome/browser/chromeos/crostini/crostini_util.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/views/crostini/crostini_browser_test_util.h"
#include "chrome/grit/generated_resources.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/views/window/dialog_client_view.h"

class CrostiniAnsibleSoftwareConfigViewBrowserTest
    : public CrostiniDialogBrowserTest {
 public:
  CrostiniAnsibleSoftwareConfigViewBrowserTest()
      : CrostiniDialogBrowserTest(true /*register_termina*/) {}

  // CrostiniDialogBrowserTest:
  void ShowUi(const std::string& name) override {
    crostini::ShowCrostiniAnsibleSoftwareConfigView(browser()->profile());
  }

  CrostiniAnsibleSoftwareConfigView* ActiveView() {
    return CrostiniAnsibleSoftwareConfigView::GetActiveViewForTesting();
  }

 protected:
  // A new Widget was created in ShowUi() or since the last VerifyUi().
  bool HasView() { return VerifyUi() && ActiveView() != nullptr; }

  // No new Widget was created in ShowUi() or since last VerifyUi().
  bool HasNoView() {
    base::RunLoop().RunUntilIdle();
    return !VerifyUi() && ActiveView() == nullptr;
  }

  bool IsDefaultDialog() { return !HasAcceptButton() && HasDefaultStrings(); }

  bool IsErrorDialog() { return HasAcceptButton() && HasErrorStrings(); }

 private:
  bool HasAcceptButton() {
    return ActiveView()->GetDialogClientView()->ok_button() != nullptr;
  }

  bool HasDefaultStrings() {
    return (ActiveView()->GetWindowTitle().compare(l10n_util::GetStringUTF16(
                IDS_CROSTINI_ANSIBLE_SOFTWARE_CONFIG_LABEL)) == 0) &&
           (ActiveView()->GetSubtextLabelStringForTesting().compare(
                l10n_util::GetStringUTF16(
                    IDS_CROSTINI_ANSIBLE_SOFTWARE_CONFIG_SUBTEXT)) == 0);
  }

  bool HasErrorStrings() {
    return (ActiveView()->GetWindowTitle().compare(l10n_util::GetStringUTF16(
                IDS_CROSTINI_ANSIBLE_SOFTWARE_CONFIG_ERROR_LABEL)) == 0) &&
           (ActiveView()->GetSubtextLabelStringForTesting().compare(
                l10n_util::GetStringUTF16(
                    IDS_CROSTINI_ANSIBLE_SOFTWARE_CONFIG_ERROR_SUBTEXT)) == 0);
  }
};

// Test if dialog is actually launched.
IN_PROC_BROWSER_TEST_F(CrostiniAnsibleSoftwareConfigViewBrowserTest,
                       InvokeUi_default) {
  ShowAndVerifyUi();
}

// Test if dialog behaviour is correct during a successful flow.
IN_PROC_BROWSER_TEST_F(CrostiniAnsibleSoftwareConfigViewBrowserTest,
                       SuccessfulFlow) {
  ShowUi("default");

  EXPECT_TRUE(HasView());
  EXPECT_TRUE(IsDefaultDialog());

  ActiveView()->OnAnsibleSoftwareConfigurationFinished(true);

  EXPECT_TRUE(HasNoView());
}

// Test if dialog behaviour is correct during an unsuccessful flow.
IN_PROC_BROWSER_TEST_F(CrostiniAnsibleSoftwareConfigViewBrowserTest,
                       UnsuccessfulFlow) {
  ShowUi("default");

  EXPECT_TRUE(HasView());
  EXPECT_TRUE(IsDefaultDialog());

  ActiveView()->OnAnsibleSoftwareConfigurationFinished(false);

  EXPECT_NE(nullptr, ActiveView());
  EXPECT_TRUE(IsErrorDialog());
}
