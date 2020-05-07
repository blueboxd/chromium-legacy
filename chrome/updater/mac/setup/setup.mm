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
#import "chrome/updater/mac/setup/info_plist.h"
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
bool CopyBundle(const base::FilePath& dest_path) {
  if (!base::PathExists(dest_path)) {
    base::File::Error error;
    if (!base::CreateDirectoryAndGetError(dest_path, &error)) {
      LOG(ERROR) << "Failed to create '" << dest_path.value().c_str()
                 << "' directory: " << base::File::ErrorToString(error);
      return false;
    }
  }

  if (!base::CopyDirectory(base::mac::OuterBundlePath(), dest_path, true)) {
    LOG(ERROR) << "Copying app to '" << dest_path.value().c_str() << "' failed";
    return false;
  }
  return true;
}

NSString* MakeProgramArgument(const char* argument) {
  return base::SysUTF8ToNSString(base::StrCat({"--", argument}));
}

base::ScopedCFTypeRef<CFDictionaryRef> CreateGoogleUpdateCheckLaunchdPlist(
    const base::ScopedCFTypeRef<CFStringRef> label,
    const base::FilePath& updater_path) {
  // See the man page for launchd.plist.
  NSMutableArray* programArguments = [NSMutableArray array];
  [programArguments addObjectsFromArray:@[
    base::SysUTF8ToNSString(updater_path.value()),
    MakeProgramArgument(kUpdateAppsSwitch)
  ]];
  if (IsSystemInstall())
    [programArguments addObject:MakeProgramArgument(kSystemSwitch)];

  NSDictionary* launchd_plist = @{
    @LAUNCH_JOBKEY_LABEL : base::mac::CFToNSCast(label),
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
    const base::ScopedCFTypeRef<CFStringRef> label,
    const base::FilePath& updater_path) {
  // See the man page for launchd.plist.
  NSDictionary* launchd_plist = @{
    @LAUNCH_JOBKEY_LABEL : base::mac::CFToNSCast(label),
    @LAUNCH_JOBKEY_PROGRAMARGUMENTS : @[
      base::SysUTF8ToNSString(updater_path.value()),
      MakeProgramArgument(kServerSwitch)
    ],
    @LAUNCH_JOBKEY_MACHSERVICES :
        @{GetGoogleUpdateServiceMachName(base::mac::CFToNSCast(label)) : @YES},
    @LAUNCH_JOBKEY_ABANDONPROCESSGROUP : @NO,
    @LAUNCH_JOBKEY_LIMITLOADTOSESSIONTYPE : @"Aqua"
  };

  return base::ScopedCFTypeRef<CFDictionaryRef>(
      base::mac::CFCast<CFDictionaryRef>(launchd_plist),
      base::scoped_policy::RETAIN);
}

bool CreateLaunchdCheckItem(const base::ScopedCFTypeRef<CFStringRef> name,
                            const base::FilePath& updater_path) {
  // We're creating directories and writing a file.
  base::ScopedBlockingCall scoped_blocking_call(FROM_HERE,
                                                base::BlockingType::MAY_BLOCK);

  base::ScopedCFTypeRef<CFDictionaryRef> plist(
      CreateGoogleUpdateCheckLaunchdPlist(name, updater_path));
  return Launchd::GetInstance()->WritePlistToFile(
      LaunchdDomain(), UpdateCheckLaunchdType(), name, plist);
}

bool CreateLaunchdServiceItem(const base::ScopedCFTypeRef<CFStringRef> name,
                              const base::FilePath& updater_path) {
  // We're creating directories and writing a file.
  base::ScopedBlockingCall scoped_blocking_call(FROM_HERE,
                                                base::BlockingType::MAY_BLOCK);

  base::ScopedCFTypeRef<CFDictionaryRef> plist(
      CreateGoogleUpdateServiceLaunchdPlist(name, updater_path));
  return Launchd::GetInstance()->WritePlistToFile(
      LaunchdDomain(), ServiceLaunchdType(), name, plist);
}

bool StartLaunchdUpdateCheckVersionedTask(
    const base::ScopedCFTypeRef<CFStringRef> name) {
  return Launchd::GetInstance()->RestartJob(
      LaunchdDomain(), UpdateCheckLaunchdType(), name, CFSTR("Aqua"));
}

bool StartLaunchdServiceVersionedTask(
    const base::ScopedCFTypeRef<CFStringRef> name) {
  return Launchd::GetInstance()->RestartJob(
      LaunchdDomain(), ServiceLaunchdType(), name, CFSTR("Aqua"));
}

bool StartLaunchdServiceTask() {
  return StartLaunchdServiceVersionedTask(CopyGoogleUpdateServiceLaunchDName());
}

bool RemoveUpdateCheckFromLaunchd(base::ScopedCFTypeRef<CFStringRef> name) {
  // This may block while deleting the launchd plist file.
  base::ScopedBlockingCall scoped_blocking_call(FROM_HERE,
                                                base::BlockingType::MAY_BLOCK);
  Launchd::GetInstance()->RemoveJob(base::SysCFStringRefToUTF8(name));
  return Launchd::GetInstance()->DeletePlist(LaunchdDomain(),
                                             UpdateCheckLaunchdType(), name);
}

bool RemoveServiceFromLaunchd(base::ScopedCFTypeRef<CFStringRef> name) {
  // This may block while deleting the launchd plist file.
  base::ScopedBlockingCall scoped_blocking_call(FROM_HERE,
                                                base::BlockingType::MAY_BLOCK);
  Launchd::GetInstance()->RemoveJob(base::SysCFStringRefToUTF8(name));
  return Launchd::GetInstance()->DeletePlist(LaunchdDomain(),
                                             ServiceLaunchdType(), name);
}

bool RemoveServiceFromLaunchd() {
  return RemoveServiceFromLaunchd(CopyGoogleUpdateServiceLaunchDName());
}

bool DeleteInstallFolder(const base::FilePath& installed_path) {
  if (!base::DeleteFileRecursively(installed_path)) {
    LOG(ERROR) << "Deleting " << installed_path << " failed";
    return false;
  }
  return true;
}

bool DeleteInstallFolder() {
  return DeleteInstallFolder(GetUpdaterFolderPath());
}

void CleanUpOldUpdater(const InfoPlist& old_updater_info_plist) {
  RemoveUpdateCheckFromLaunchd(
      old_updater_info_plist.GoogleUpdateCheckLaunchdNameVersioned());
  RemoveServiceFromLaunchd(
      old_updater_info_plist.GoogleUpdateServiceLaunchdNameVersioned());
  DeleteInstallFolder(old_updater_info_plist.UpdaterVersionedFolderPath(
      GetUpdaterFolderPath()));
}

}  // namespace

int SetupUpdater() {
  const std::unique_ptr<InfoPlist> info_plist =
      InfoPlist::Create(InfoPlistPath());
  CHECK(info_plist);

  const base::FilePath dest_path =
      info_plist->UpdaterVersionedFolderPath(GetUpdaterFolderPath());

  if (!CopyBundle(dest_path))
    return -1;

  base::FilePath updater_executable_path = info_plist->UpdaterExecutablePath(
      GetLibraryFolderPath(), GetUpdateFolderName(), GetUpdaterAppName(),
      GetUpdaterAppExecutablePath());

  if (!CreateLaunchdCheckItem(
          info_plist->GoogleUpdateCheckLaunchdNameVersioned(),
          updater_executable_path)) {
    return -2;
  }

  if (!CreateLaunchdServiceItem(
          info_plist->GoogleUpdateServiceLaunchdNameVersioned(),
          updater_executable_path)) {
    return -3;
  }

  if (!StartLaunchdUpdateCheckVersionedTask(
          info_plist->GoogleUpdateCheckLaunchdNameVersioned())) {
    return -4;
  }

  if (!StartLaunchdServiceVersionedTask(
          info_plist->GoogleUpdateServiceLaunchdNameVersioned())) {
    return -5;
  }

  return 0;
}

int SwapToUpgradedUpdater() {
  const std::unique_ptr<InfoPlist> info_plist =
      InfoPlist::Create(InfoPlistPath());
  CHECK(info_plist);

  // Check if a version of the updater already exists.
  const bool old_updater_exists =
      Launchd::GetInstance()->PlistExists(LaunchdDomain(), ServiceLaunchdType(),
                                          CopyGoogleUpdateServiceLaunchDName());
  base::scoped_nsobject<NSString> old_updater_bundle_path;
  if (old_updater_exists) {
    const std::string service_label =
        base::SysNSStringToUTF8(GetGoogleUpdateServiceLaunchDLabel());
    mac::services::JobInfo info;
    if (Launchd::GetInstance()->GetJobInfo(service_label, &info)) {
      base::scoped_nsobject<NSString> executable_path(
          base::SysUTF8ToNSString(info.program), base::scoped_policy::RETAIN);
      if (executable_path) {
        // Get the app bundle path from the executable path.
        // '<path>/<version>/GoogleUpdater.app' from
        // '<path>/<version>/GoogleUpdater.app/Contents/MacOS/GoogleUpdater'
        old_updater_bundle_path.reset(
            [[[executable_path stringByDeletingLastPathComponent]
                stringByDeletingLastPathComponent]
                stringByDeletingLastPathComponent],
            base::scoped_policy::RETAIN);
      } else {
        DLOG(ERROR) << "Failed to get executable path for service process.";
      }
    } else {
      DLOG(ERROR) << "Failed to get job info for: " << service_label;
    }
  }

  // Create an unversioned launchd updater service plist. Located at a path
  // that is static and doesn't include any version. The name of the service is
  // also static and doesn't include the updater's version, so that it can be
  // discovered by other applications.
  const base::FilePath updater_executable_path =
      info_plist->UpdaterExecutablePath(
          GetLibraryFolderPath(), GetUpdateFolderName(), GetUpdaterAppName(),
          GetUpdaterAppExecutablePath());

  if (!CreateLaunchdServiceItem(CopyGoogleUpdateServiceLaunchDName(),
                                updater_executable_path)) {
    return -1;
  }

  if (!StartLaunchdServiceTask())
    return -2;

  if (old_updater_exists) {
    if (old_updater_bundle_path.get()) {
      const base::FilePath old_info_plist_path =
          InfoPlistPath(base::mac::NSStringToFilePath(old_updater_bundle_path));
      const std::unique_ptr<InfoPlist> old_info_plist =
          InfoPlist::Create(old_info_plist_path);
      if (old_info_plist != nullptr) {
        if (![old_info_plist->BundleVersion()
                isEqualToString:info_plist->BundleVersion()]) {
          CleanUpOldUpdater(*old_info_plist);
        } else {
          LOG(ERROR) << "Old version and new version are the same: "
                     << base::SysNSStringToUTF8(
                            old_info_plist->BundleVersion());
        }
      } else {
        LOG(ERROR) << "Failed to get info plist at: " << old_info_plist_path;
      }
    }
  }

  return 0;
}

#pragma mark Uninstall
int Uninstall(bool is_machine) {
  ALLOW_UNUSED_LOCAL(is_machine);

  const std::unique_ptr<InfoPlist> info_plist =
      InfoPlist::Create(InfoPlistPath());
  CHECK(info_plist);

  if (!RemoveUpdateCheckFromLaunchd(
          info_plist->GoogleUpdateCheckLaunchdNameVersioned())) {
    return -1;
  }

  if (!RemoveServiceFromLaunchd(
          info_plist->GoogleUpdateServiceLaunchdNameVersioned())) {
    return -2;
  }

  if (!RemoveServiceFromLaunchd())
    return -3;

  if (!DeleteInstallFolder())
    return -4;

  return 0;
}

}  // namespace updater
