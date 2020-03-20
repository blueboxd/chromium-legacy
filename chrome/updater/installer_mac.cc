// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/updater/installer.h"

#include "base/logging.h"
#include "base/strings/strcat.h"
#include "chrome/updater/mac/installer.h"

namespace updater {

int Installer::RunApplicationInstaller(const base::FilePath& app_installer,
                                       const std::string& arguments) {
  DVLOG(1) << "Running application install from DMG";
  // InstallFromDMG() returns the exit code of the script. 0 is success and
  // anything else should be an error.
  return InstallFromDMG(
      app_installer,
      base::StrCat({persisted_data_->GetExistenceCheckerPath(app_id_).value(),
                    " ", arguments}));
}

}  // namespace updater
