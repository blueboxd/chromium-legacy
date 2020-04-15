// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/updater/mac/setup/setup.h"

#import <ServiceManagement/ServiceManagement.h>

#include "base/at_exit.h"
#include "base/command_line.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/logging.h"
#include "base/mac/bundle_locations.h"
#include "base/mac/foundation_util.h"
#include "base/mac/scoped_nsobject.h"
#include "base/path_service.h"
#include "base/strings/strcat.h"
#include "base/strings/sys_string_conversions.h"
#include "base/task/thread_pool/thread_pool_instance.h"
#include "base/threading/scoped_blocking_call.h"
#include "build/build_config.h"
#include "chrome/common/mac/launchd.h"
#include "chrome/updater/constants.h"
#include "chrome/updater/crash_client.h"
#include "chrome/updater/crash_reporter.h"
#import "chrome/updater/mac/xpc_service_names.h"
#include "chrome/updater/updater_version.h"
#include "chrome/updater/util.h"
#include "components/crash/core/common/crash_key.h"

namespace updater {

namespace {

#pragma mark Helpers
const base::FilePath GetUpdateFolderName() {
  return base::FilePath(COMPANY_SHORTNAME_STRING)
      .AppendASCII(PRODUCT_FULLNAME_STRING);
}

const base::FilePath GetUpdaterAppName() {
  return base::FilePath(PRODUCT_FULLNAME_STRING ".app");
}

const base::FilePath GetUpdaterAppExecutablePath() {
  return base::FilePath("Contents/MacOS").AppendASCII(PRODUCT_FULLNAME_STRING);
}

bool IsSystemInstall() {
  return geteuid() == 0;
}

const base::FilePath GetLocalLibraryDirectory() {
  base::FilePath local_library_path;
  if (!base::mac::GetLocalDirectory(NSLibraryDirectory, &local_library_path)) {
    DLOG(WARNING) << "Could not get local library path";
  }
  return local_library_path;
}

const base::FilePath GetLibraryFolderPath() {
  // For user installations: the "~/Library" for the logged in user.
  // For system installations: "/Library".
  return IsSystemInstall() ? GetLocalLibraryDirectory()
                           : base::mac::GetUserLibraryPath();
}

const base::FilePath GetUpdaterFolderPath() {
  // For user installations:
  // ~/Library/COMPANY_SHORTNAME_STRING/PRODUCT_FULLNAME_STRING.
  // e.g. ~/Library/Google/GoogleUpdater
  // For system installations:
  // /Library/COMPANY_SHORTNAME_STRING/PRODUCT_FULLNAME_STRING.
  // e.g. /Library/Google/GoogleUpdater
  return GetLibraryFolderPath().Append(GetUpdateFolderName());
}

const base::FilePath GetUpdaterExecutablePath() {
  return GetLibraryFolderPath()
      .Append(GetUpdateFolderName())
      .Append(GetUpdaterAppName())
      .Append(GetUpdaterAppExecutablePath());
}

Launchd::Domain LaunchdDomain() {
  return IsSystemInstall() ? Launchd::Domain::Local : Launchd::Domain::User;
}

Launchd::Type ServiceLaunchdType() {
  return IsSystemInstall() ? Launchd::Type::Daemon : Launchd::Type::Agent;
}

Launchd::Type UpdateCheckLaunchdType() {
  return Launchd::Type::Agent;
}

#pragma mark Setup
bool CopyBundle() {
  const base::FilePath dest_path = GetUpdaterFolderPath();

  if (!base::PathExists(dest_path)) {
    base::File::Error error;
    if (!base::CreateDirectoryAndGetError(dest_path, &error)) {
      LOG(ERROR) << "Failed to create '" << dest_path.value().c_str()
                 << "' directory: " << base::File::ErrorToString(error);
      return false;
    }
  }

  const base::FilePath src_path = base::mac::OuterBundlePath();
  if (!base::CopyDirectory(src_path, dest_path, true)) {
    LOG(ERROR) << "Copying app to '" << dest_path.value().c_str() << "' failed";
    return false;
  }
  return true;
}

NSString* MakeProgramArgument(const char* argument) {
  return base::SysUTF8ToNSString(base::StrCat({"--", argument}));
}

base::ScopedCFTypeRef<CFDictionaryRef> CreateGoogleUpdateCheckLaunchdPlist(
    const base::FilePath* updater_path) {
  // See the man page for launchd.plist.
  NSMutableArray* programArguments = [NSMutableArray array];
  [programArguments addObjectsFromArray:@[
    base::SysUTF8ToNSString(updater_path->value()),
    MakeProgramArgument(kUpdateAppsSwitch)
  ]];
  if (IsSystemInstall())
    [programArguments addObject:MakeProgramArgument(kSystemSwitch)];

  NSDictionary* launchd_plist = @{
    @LAUNCH_JOBKEY_LABEL : GetGoogleUpdateCheckLaunchDLabel(),
    @LAUNCH_JOBKEY_PROGRAMARGUMENTS : programArguments,
    @LAUNCH_JOBKEY_STARTINTERVAL : @18000,
    @LAUNCH_JOBKEY_ABANDONPROCESSGROUP : @NO,
    @LAUNCH_JOBKEY_LIMITLOADTOSESSIONTYPE : @"Aqua"
  };

  return base::ScopedCFTypeRef<CFDictionaryRef>(
      base::mac::CFCast<CFDictionaryRef>(launchd_plist),
      base::scoped_policy::RETAIN);
}

base::ScopedCFTypeRef<CFDictionaryRef> CreateGoogleUpdateServiceLaunchdPlist(
    const base::FilePath* updater_path) {
  // See the man page for launchd.plist.
  NSDictionary* launchd_plist = @{
    @LAUNCH_JOBKEY_LABEL : GetGoogleUpdateServiceLaunchDLabel(),
    @LAUNCH_JOBKEY_PROGRAMARGUMENTS : @[
      base::SysUTF8ToNSString(updater_path->value()),
      MakeProgramArgument(kServerSwitch)
    ],
    @LAUNCH_JOBKEY_MACHSERVICES : @{GetGoogleUpdateServiceMachName() : @YES},
    @LAUNCH_JOBKEY_ABANDONPROCESSGROUP : @NO,
    @LAUNCH_JOBKEY_LIMITLOADTOSESSIONTYPE : @"Aqua"
  };

  return base::ScopedCFTypeRef<CFDictionaryRef>(
      base::mac::CFCast<CFDictionaryRef>(launchd_plist),
      base::scoped_policy::RETAIN);
}

bool CreateLaunchdCheckItem() {
  // We're creating directories and writing a file.
  base::ScopedBlockingCall scoped_blocking_call(FROM_HERE,
                                                base::BlockingType::MAY_BLOCK);
  base::ScopedCFTypeRef<CFStringRef> name(CopyGoogleUpdateCheckLaunchDName());

  const base::FilePath updater_path = GetUpdaterExecutablePath();

  base::ScopedCFTypeRef<CFDictionaryRef> plist(
      CreateGoogleUpdateCheckLaunchdPlist(&updater_path));
  return Launchd::GetInstance()->WritePlistToFile(
      LaunchdDomain(), UpdateCheckLaunchdType(), name, plist);
}

bool CreateLaunchdServiceItem() {
  // We're creating directories and writing a file.
  base::ScopedBlockingCall scoped_blocking_call(FROM_HERE,
                                                base::BlockingType::MAY_BLOCK);
  base::ScopedCFTypeRef<CFStringRef> name(CopyGoogleUpdateServiceLaunchDName());

  const base::FilePath updater_path = GetUpdaterExecutablePath();

  base::ScopedCFTypeRef<CFDictionaryRef> plist(
      CreateGoogleUpdateServiceLaunchdPlist(&updater_path));
  return Launchd::GetInstance()->WritePlistToFile(
      LaunchdDomain(), ServiceLaunchdType(), name, plist);
}

bool StartLaunchdUpdateCheckTask() {
  base::ScopedCFTypeRef<CFStringRef> name(CopyGoogleUpdateCheckLaunchDName());
  return Launchd::GetInstance()->RestartJob(
      LaunchdDomain(), UpdateCheckLaunchdType(), name, CFSTR("Aqua"));
}

bool StartLaunchdServiceTask() {
  base::ScopedCFTypeRef<CFStringRef> name(CopyGoogleUpdateServiceLaunchDName());
  return Launchd::GetInstance()->RestartJob(
      LaunchdDomain(), ServiceLaunchdType(), name, CFSTR("Aqua"));
}

}  // namespace

int SetupUpdater() {
  if (!CopyBundle())
    return -1;

  if (!CreateLaunchdCheckItem())
    return -2;

  if (!CreateLaunchdServiceItem())
    return -3;

  if (!StartLaunchdUpdateCheckTask())
    return -4;

  if (!StartLaunchdServiceTask())
    return -5;

  return 0;
}

#pragma mark Uninstall
bool RemoveUpdateCheckFromLaunchd() {
  // This may block while deleting the launchd plist file.
  base::ScopedBlockingCall scoped_blocking_call(FROM_HERE,
                                                base::BlockingType::MAY_BLOCK);
  base::ScopedCFTypeRef<CFStringRef> name(CopyGoogleUpdateCheckLaunchDName());
  return Launchd::GetInstance()->DeletePlist(LaunchdDomain(),
                                             UpdateCheckLaunchdType(), name);
}

bool RemoveServiceFromLaunchd() {
  // This may block while deleting the launchd plist file.
  base::ScopedBlockingCall scoped_blocking_call(FROM_HERE,
                                                base::BlockingType::MAY_BLOCK);
  base::ScopedCFTypeRef<CFStringRef> name(CopyGoogleUpdateServiceLaunchDName());
  return Launchd::GetInstance()->DeletePlist(LaunchdDomain(),
                                             ServiceLaunchdType(), name);
}

bool DeleteInstallFolder() {
  const base::FilePath dest_path = GetUpdaterFolderPath();

  if (!base::DeleteFileRecursively(dest_path)) {
    LOG(ERROR) << "Deleting " << dest_path << " failed";
    return false;
  }
  return true;
}

int Uninstall(bool is_machine) {
  ALLOW_UNUSED_LOCAL(is_machine);
  if (!RemoveUpdateCheckFromLaunchd())
    return -1;

  if (!RemoveServiceFromLaunchd())
    return -2;

  if (!DeleteInstallFolder())
    return -3;

  return 0;
}

}  // namespace updater
