// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/updater/installer.h"

#include "base/logging.h"
#include "chrome/updater/mac/installer.h"

namespace updater {

int Installer::RunApplicationInstaller(const base::FilePath& app_installer,
                                       const std::string& arguments) {
  DVLOG(1) << "Running application install from DMG";
  // InstallFromDMG() returns true when it succeeds while callers of
  // RunApplicationInstaller() expect a return value of 0.
  return !InstallFromDMG(app_installer, arguments);
}

}  // namespace updater
