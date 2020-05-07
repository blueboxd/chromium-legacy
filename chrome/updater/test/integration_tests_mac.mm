// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/command_line.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/mac/foundation_util.h"
#include "base/path_service.h"
#include "base/process/launch.h"
#include "base/process/process.h"
#include "chrome/common/mac/launchd.h"
#include "chrome/updater/constants.h"
#include "chrome/updater/mac/setup/info_plist.h"
#include "chrome/updater/mac/xpc_service_names.h"
#include "chrome/updater/updater_version.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace updater {

namespace test {

namespace {

base::FilePath GetInfoPlistPath() {
  base::FilePath test_executable;
  if (!base::PathService::Get(base::FILE_EXE, &test_executable))
    return base::FilePath();
  return test_executable.DirName()
      .Append(FILE_PATH_LITERAL(PRODUCT_FULLNAME_STRING ".App"))
      .Append(FILE_PATH_LITERAL("Contents"))
      .Append(FILE_PATH_LITERAL("Info.plist"));
}

base::FilePath GetExecutablePath() {
  base::FilePath test_executable;
  if (!base::PathService::Get(base::FILE_EXE, &test_executable))
    return base::FilePath();
  return test_executable.DirName()
      .Append(FILE_PATH_LITERAL(PRODUCT_FULLNAME_STRING ".App"))
      .Append(FILE_PATH_LITERAL("Contents"))
      .Append(FILE_PATH_LITERAL("MacOS"))
      .Append(FILE_PATH_LITERAL(PRODUCT_FULLNAME_STRING));
}

base::FilePath GetProductPath() {
  return base::mac::GetUserLibraryPath()
      .AppendASCII(COMPANY_SHORTNAME_STRING)
      .AppendASCII(PRODUCT_FULLNAME_STRING);
}

bool Run(base::CommandLine command_line, int* exit_code) {
  auto process = base::LaunchProcess(command_line, {});
  if (!process.IsValid())
    return false;
  if (!process.WaitForExitWithTimeout(base::TimeDelta::FromSeconds(60),
                                      exit_code))
    return false;
  return true;
}

}  // namespace

void Clean() {
  const std::unique_ptr<InfoPlist> info_plist =
      InfoPlist::Create(GetInfoPlistPath());
  EXPECT_TRUE(info_plist != nullptr);

  EXPECT_TRUE(base::DeleteFile(GetProductPath(), true));
  EXPECT_TRUE(Launchd::GetInstance()->DeletePlist(
      Launchd::User, Launchd::Agent,
      info_plist->GoogleUpdateCheckLaunchdNameVersioned()));
  EXPECT_TRUE(Launchd::GetInstance()->DeletePlist(
      Launchd::User, Launchd::Agent,
      info_plist->GoogleUpdateServiceLaunchdNameVersioned()));
  EXPECT_TRUE(Launchd::GetInstance()->DeletePlist(
      Launchd::User, Launchd::Agent,
      updater::CopyGoogleUpdateServiceLaunchDName()));
}

void ExpectClean() {
  const std::unique_ptr<InfoPlist> info_plist =
      InfoPlist::Create(GetInfoPlistPath());
  EXPECT_TRUE(info_plist != nullptr);

  // Files must not exist on the file system.
  EXPECT_FALSE(base::PathExists(GetProductPath()));
  EXPECT_FALSE(Launchd::GetInstance()->PlistExists(
      Launchd::User, Launchd::Agent,
      info_plist->GoogleUpdateCheckLaunchdNameVersioned()));
  EXPECT_FALSE(Launchd::GetInstance()->PlistExists(
      Launchd::User, Launchd::Agent,
      info_plist->GoogleUpdateServiceLaunchdNameVersioned()));
  EXPECT_FALSE(Launchd::GetInstance()->PlistExists(
      Launchd::User, Launchd::Agent,
      updater::CopyGoogleUpdateCheckLaunchDName()));
}

void ExpectInstalled() {
  const std::unique_ptr<InfoPlist> info_plist =
      InfoPlist::Create(GetInfoPlistPath());
  EXPECT_TRUE(info_plist != nullptr);

  // Files must exist on the file system.
  EXPECT_TRUE(base::PathExists(GetProductPath()));
  EXPECT_FALSE(Launchd::GetInstance()->PlistExists(
      Launchd::User, Launchd::Agent, CopyGoogleUpdateServiceLaunchDName()));
  EXPECT_TRUE(Launchd::GetInstance()->PlistExists(
      Launchd::User, Launchd::Agent,
      info_plist->GoogleUpdateCheckLaunchdNameVersioned()));
  EXPECT_TRUE(Launchd::GetInstance()->PlistExists(
      Launchd::User, Launchd::Agent,
      info_plist->GoogleUpdateServiceLaunchdNameVersioned()));
}

void Install() {
  const base::FilePath path = GetExecutablePath();
  ASSERT_FALSE(path.empty());
  base::CommandLine command_line(path);
  command_line.AppendSwitch(kInstallSwitch);
  int exit_code = -1;
  ASSERT_TRUE(Run(command_line, &exit_code));
  EXPECT_EQ(0, exit_code);
}

void ExpectSwapped() {
  const std::unique_ptr<InfoPlist> info_plist =
      InfoPlist::Create(GetInfoPlistPath());
  EXPECT_TRUE(info_plist != nullptr);

  // Files must exist on the file system.
  EXPECT_TRUE(base::PathExists(GetProductPath()));
  EXPECT_TRUE(Launchd::GetInstance()->PlistExists(
      Launchd::User, Launchd::Agent,
      info_plist->GoogleUpdateCheckLaunchdNameVersioned()));
  EXPECT_TRUE(Launchd::GetInstance()->PlistExists(
      Launchd::User, Launchd::Agent, CopyGoogleUpdateServiceLaunchDName()));
  EXPECT_TRUE(Launchd::GetInstance()->PlistExists(
      Launchd::User, Launchd::Agent,
      info_plist->GoogleUpdateServiceLaunchdNameVersioned()));
}

void Swap() {
  const base::FilePath path = GetExecutablePath();
  ASSERT_FALSE(path.empty());
  base::CommandLine command_line(path);
  command_line.AppendSwitch(kSwapUpdaterSwitch);
  int exit_code = -1;
  ASSERT_TRUE(Run(command_line, &exit_code));
  EXPECT_EQ(0, exit_code);
}

void Uninstall() {
  const base::FilePath path = GetExecutablePath();
  ASSERT_FALSE(path.empty());
  base::CommandLine command_line(path);
  command_line.AppendSwitch(kUninstallSwitch);
  int exit_code = -1;
  ASSERT_TRUE(Run(command_line, &exit_code));
  EXPECT_EQ(0, exit_code);
}

}  // namespace test

}  // namespace updater
