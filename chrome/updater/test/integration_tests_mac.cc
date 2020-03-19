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
#include "chrome/updater/updater_version.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace updater {

namespace test {

namespace {

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

base::FilePath GetInstallerPath() {
  base::FilePath test_executable;
  if (!base::PathService::Get(base::FILE_EXE, &test_executable))
    return base::FilePath();
  return test_executable.DirName().Append("updater_setup");
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
  EXPECT_TRUE(base::DeleteFile(base::mac::GetUserLibraryPath()
                                   .AppendASCII(COMPANY_SHORTNAME_STRING)
                                   .AppendASCII(PRODUCT_FULLNAME_STRING),
                               true));
  // TODO(crbug.com/1062288): Delete the service launchd entry.
  // TODO(crbug.com/1062288): Delete the update task launchd entry.
}

void ExpectClean() {
  // Files must not exist on the file system.
  EXPECT_FALSE(base::PathExists(base::mac::GetUserLibraryPath()
                                    .AppendASCII(COMPANY_SHORTNAME_STRING)
                                    .AppendASCII(PRODUCT_FULLNAME_STRING)));
  // TODO(crbug.com/1062288): Check that service Launchd entry does not exist.
  // TODO(crbug.com/1062288): Check that update task Launchd entry does not
  // exist.
}

void ExpectInstalled() {
  // Files must exist on the file system.
  EXPECT_TRUE(base::PathExists(base::mac::GetUserLibraryPath()
                                   .AppendASCII(COMPANY_SHORTNAME_STRING)
                                   .AppendASCII(PRODUCT_FULLNAME_STRING)));
  // TODO(crbug.com/1062288): Check that service Launchd entry exists.
  // TODO(crbug.com/1062288): Check that update task Launchd entry exists.
}

void Install() {
  base::FilePath path = GetInstallerPath();
  ASSERT_FALSE(path.empty());
  int exit_code = -1;
  ASSERT_TRUE(Run(base::CommandLine(path), &exit_code));
  EXPECT_EQ(0, exit_code);
}

void Uninstall() {
  base::FilePath path = GetExecutablePath();
  ASSERT_FALSE(path.empty());
  base::CommandLine command_line(path);
  command_line.AppendSwitch("uninstall");
  int exit_code = -1;
  ASSERT_TRUE(Run(command_line, &exit_code));
  EXPECT_EQ(0, exit_code);
}

}  // namespace test

}  // namespace updater
