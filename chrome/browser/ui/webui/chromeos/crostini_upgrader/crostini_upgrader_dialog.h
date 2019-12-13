// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_CHROMEOS_CROSTINI_UPGRADER_CROSTINI_UPGRADER_DIALOG_H_
#define CHROME_BROWSER_UI_WEBUI_CHROMEOS_CROSTINI_UPGRADER_CROSTINI_UPGRADER_DIALOG_H_

#include "chrome/browser/ui/webui/chromeos/system_web_dialog_delegate.h"

class Profile;

namespace chromeos {

class CrostiniUpgraderUI;

class CrostiniUpgraderDialog : public SystemWebDialogDelegate {
 public:
  static void Show(Profile* profile, base::OnceClosure launch_closure);

 private:
  explicit CrostiniUpgraderDialog(base::OnceClosure launch_closure);
  ~CrostiniUpgraderDialog() override;

  // SystemWebDialogDelegate:
  void GetDialogSize(gfx::Size* size) const override;
  bool ShouldShowCloseButton() const override;
  void AdjustWidgetInitParams(views::Widget::InitParams* params) override;
  bool CanCloseDialog() const override;
  void OnDialogShown(content::WebUI* webui) override;
  void OnCloseContents(content::WebContents* source,
                       bool* out_close_dialog) override;

 private:
  // Not owned.
  CrostiniUpgraderUI* upgrader_ui_ = nullptr;
  base::OnceClosure launch_closure_;
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_UI_WEBUI_CHROMEOS_CROSTINI_UPGRADER_CROSTINI_UPGRADER_DIALOG_H_
