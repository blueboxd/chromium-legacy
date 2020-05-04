// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/updater/test/test_app/test_app.h"

#import <Cocoa/Cocoa.h>

#include <string>

#include "base/command_line.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/mac/bundle_locations.h"
#import "base/mac/foundation_util.h"
#include "base/process/launch.h"
#import "chrome/updater/mac/setup/info_plist.h"
#include "chrome/updater/mac/xpc_service_names.h"
#include "chrome/updater/test/test_app/constants.h"
#include "chrome/updater/test/test_app/test_app_version.h"

namespace updater {

namespace {

constexpr char kInstallCommand[] = "install";
constexpr char kSwapAppCommand[] = "swap-updater";

base::FilePath GetUpdaterAppName() {
  return base::FilePath(UPDATER_APP_FULLNAME_STRING ".app");
}

base::FilePath GetTestAppFrameworkName() {
  return base::FilePath(TEST_APP_FULLNAME_STRING " Framework.framework");
}

base::FilePath GetUpdateFolderName() {
  return base::FilePath(TEST_APP_COMPANY_NAME_STRING)
      .AppendASCII(UPDATER_APP_FULLNAME_STRING);
}

base::FilePath GetUpdaterAppExecutablePath() {
  return base::FilePath("Contents/MacOS")
      .AppendASCII(UPDATER_APP_FULLNAME_STRING);
}

void SwapUpdater(const base::FilePath& updater_bundle_path) {
  base::FilePath plist_path =
      updater_bundle_path.Append("Contents").Append("Info.plist");
  std::unique_ptr<InfoPlist> plist = InfoPlist::Create(plist_path);

  base::FilePath updater_executable_path = plist->UpdaterExecutablePath(
      base::mac::GetUserLibraryPath(), GetUpdateFolderName(),
      GetUpdaterAppName(), GetUpdaterAppExecutablePath());

  base::CommandLine swap_command(updater_executable_path);
  swap_command.AppendSwitch(kSwapAppCommand);

  std::string output;
  int exit_code = 0;
  base::GetAppOutputWithExitCode(swap_command, &output, &exit_code);

  if (exit_code != 0) {
    LOG(ERROR) << "Couldn't swap the updater. Exit code: " << exit_code;
  }
}

}  // namespace

void InstallUpdater() {
  // The updater executable is in
  // C.app/Contents/Frameworks/C.framework/Versions/V/Helpers/CUpdater.app.
  base::FilePath updater_bundle_path =
      base::mac::OuterBundlePath()
          .Append(FILE_PATH_LITERAL("Contents"))
          .Append(FILE_PATH_LITERAL("Frameworks"))
          .Append(FILE_PATH_LITERAL(GetTestAppFrameworkName()))
          .Append(FILE_PATH_LITERAL("Versions"))
          .Append(FILE_PATH_LITERAL(TEST_APP_VERSION_STRING))
          .Append(FILE_PATH_LITERAL("Helpers"))
          .Append(GetUpdaterAppName());

  if (!base::PathExists(updater_bundle_path)) {
    LOG(ERROR) << "Path to the updater app does not exist!";
    return;
  }

  base::FilePath updater_executable_path =
      updater_bundle_path.Append(FILE_PATH_LITERAL("Contents"))
          .Append(FILE_PATH_LITERAL("MacOS"))
          .Append(FILE_PATH_LITERAL(UPDATER_APP_FULLNAME_STRING));

  if (!base::PathExists(updater_executable_path)) {
    LOG(ERROR) << "Path to the updater app does not exist!";
    return;
  }

  base::CommandLine command(updater_executable_path);
  command.AppendSwitch(kInstallCommand);

  std::string output;
  int exit_code = 0;
  base::GetAppOutputWithExitCode(command, &output, &exit_code);

  if (exit_code != 0) {
    LOG(ERROR) << "Couldn't install the updater. Exit code: " << exit_code;
    return;
  }

  // Now that the updater is installed successfully, we should swap.
  SwapUpdater(updater_bundle_path);
}

}  // namespace updater
