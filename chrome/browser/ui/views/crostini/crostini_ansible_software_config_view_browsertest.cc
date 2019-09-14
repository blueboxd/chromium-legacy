// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/crostini/crostini_ansible_software_config_view.h"

#include "chrome/browser/chromeos/crostini/crostini_util.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/views/crostini/crostini_browser_test_util.h"

class CrostiniAnsibleSoftwareConfigBrowserTest
    : public CrostiniDialogBrowserTest {
 public:
  CrostiniAnsibleSoftwareConfigBrowserTest()
      : CrostiniDialogBrowserTest(true /*register_termina*/) {}

  // CrostiniDialogBrowserTest:
  void ShowUi(const std::string& name) override {
    crostini::ShowCrostiniAnsibleSoftwareConfigView();
  }
};

// Test the dialog is actually launched.
IN_PROC_BROWSER_TEST_F(CrostiniAnsibleSoftwareConfigBrowserTest,
                       InvokeUi_default) {
  ShowAndVerifyUi();
}
