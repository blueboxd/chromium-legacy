// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/updater/installer.h"

#include "base/command_line.h"
#include "base/logging.h"
#include "base/process/launch.h"
#include "base/strings/strcat.h"
#include "base/strings/utf_string_conversions.h"
#include "base/time/time.h"
#include "chrome/updater/constants.h"

namespace updater {

int Installer::RunApplicationInstaller(const base::FilePath& app_installer,
                                       const std::string& arguments) {
  base::LaunchOptions options;
  options.start_hidden = true;
  const auto cmdline =
      base::StrCat({base::CommandLine(app_installer).GetCommandLineString(),
                    L" ", base::UTF8ToWide(arguments)});
  DVLOG(1) << "Running application installer: " << cmdline;
  auto process = base::LaunchProcess(cmdline, options);
  int exit_code = -1;
  process.WaitForExitWithTimeout(
      base::TimeDelta::FromSeconds(kWaitForAppInstallerSec), &exit_code);
  return exit_code;
}

}  // namespace updater
