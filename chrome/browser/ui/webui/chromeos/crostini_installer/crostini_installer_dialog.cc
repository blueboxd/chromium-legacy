// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/chromeos/crostini_installer/crostini_installer_dialog.h"

#include "chrome/browser/chromeos/crostini/crostini_manager.h"
#include "chrome/common/webui_url_constants.h"

namespace {
GURL GetUrl() {
  return GURL{chrome::kChromeUICrostiniInstallerUrl};
}
}  // namespace

namespace chromeos {

void CrostiniInstallerDialog::Show(Profile* profile) {
  DCHECK(crostini::IsCrostiniUIAllowedForProfile(profile));
  auto* instance = SystemWebDialogDelegate::FindInstance(GetUrl().spec());
  if (instance) {
    instance->Focus();
    return;
  }

  // TODO(lxj): Move installer status tracking into the CrostiniInstaller.
  DCHECK(!crostini::CrostiniManager::GetForProfile(profile)
              ->GetInstallerViewStatus());
  crostini::CrostiniManager::GetForProfile(profile)->SetInstallerViewStatus(
      true);

  instance = new CrostiniInstallerDialog(profile);
  instance->ShowSystemDialog();
}

CrostiniInstallerDialog::CrostiniInstallerDialog(Profile* profile)
    : SystemWebDialogDelegate{GetUrl(), /*title=*/{}}, profile_{profile} {}

CrostiniInstallerDialog::~CrostiniInstallerDialog() {
  crostini::CrostiniManager::GetForProfile(profile_)->SetInstallerViewStatus(
      false);
}

}  // namespace chromeos
