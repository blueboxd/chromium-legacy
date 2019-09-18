// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_CHROMEOS_CROSTINI_INSTALLER_CROSTINI_INSTALLER_DIALOG_H_
#define CHROME_BROWSER_UI_WEBUI_CHROMEOS_CROSTINI_INSTALLER_CROSTINI_INSTALLER_DIALOG_H_

#include "chrome/browser/ui/webui/chromeos/system_web_dialog_delegate.h"

class Profile;

namespace chromeos {

class CrostiniInstallerDialog : public SystemWebDialogDelegate {
 public:
  static void Show(Profile* profile);

 private:
  explicit CrostiniInstallerDialog(Profile* profile);
  ~CrostiniInstallerDialog() override;

  Profile* profile_;
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_UI_WEBUI_CHROMEOS_CROSTINI_INSTALLER_CROSTINI_INSTALLER_DIALOG_H_
