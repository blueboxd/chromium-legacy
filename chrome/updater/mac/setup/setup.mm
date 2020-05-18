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

Launchd::Type ClientLaunchdType() {
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

base::ScopedCFTypeRef<CFDictionaryRef> CreateGoogleUpdateServiceLaunchdPlist(
    const base::ScopedCFTypeRef<CFStringRef> label,
    const base::FilePath& updater_path) {
  // See the man page for launchd.plist.
  NSDictionary<NSString*, id>* launchd_plist = @{
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

base::ScopedCFTypeRef<CFDictionaryRef>
CreateGoogleUpdaterAdministrationLaunchdPlist(
    const base::ScopedCFTypeRef<CFStringRef> label,
    const base::FilePath& updater_path) {
  // See the man page for launchd.plist.
  NSMutableArray<NSString*>* program_arguments =
      [NSMutableArray<NSString*> array];
  [program_arguments addObjectsFromArray:@[
    base::SysUTF8ToNSString(updater_path.value()),
    MakeProgramArgument(kWakeSwitch)
  ]];
  if (IsSystemInstall())
    [program_arguments addObject:MakeProgramArgument(kSystemSwitch)];

  NSDictionary<NSString*, id>* launchd_plist = @{
    @LAUNCH_JOBKEY_LABEL : base::mac::CFToNSCast(label),
    @LAUNCH_JOBKEY_PROGRAMARGUMENTS : program_arguments,
    @LAUNCH_JOBKEY_STARTINTERVAL : @3600,
    @LAUNCH_JOBKEY_ABANDONPROCESSGROUP : @NO,
    @LAUNCH_JOBKEY_LIMITLOADTOSESSIONTYPE : @"Aqua"
  };

  return base::ScopedCFTypeRef<CFDictionaryRef>(
      base::mac::CFCast<CFDictionaryRef>(launchd_plist),
      base::scoped_policy::RETAIN);
}

bool CreateUpdateServiceLaunchdJobPlist(
    const base::ScopedCFTypeRef<CFStringRef> name,
    const base::FilePath& updater_path) {
  // We're creating directories and writing a file.
  base::ScopedBlockingCall scoped_blocking_call(FROM_HERE,
                                                base::BlockingType::MAY_BLOCK);

  base::ScopedCFTypeRef<CFDictionaryRef> plist(
      CreateGoogleUpdateServiceLaunchdPlist(name, updater_path));
  return Launchd::GetInstance()->WritePlistToFile(
      LaunchdDomain(), ServiceLaunchdType(), name, plist);
}

bool CreateUpdateAdministrationLaunchdJobPlist(
    const base::ScopedCFTypeRef<CFStringRef> name,
    const base::FilePath& updater_path) {
  // We're creating directories and writing a file.
  base::ScopedBlockingCall scoped_blocking_call(FROM_HERE,
                                                base::BlockingType::MAY_BLOCK);

  base::ScopedCFTypeRef<CFDictionaryRef> plist(
      CreateGoogleUpdaterAdministrationLaunchdPlist(name, updater_path));
  return Launchd::GetInstance()->WritePlistToFile(
      LaunchdDomain(), ServiceLaunchdType(), name, plist);
}

bool StartUpdateServiceVersionedLaunchdJob(
    const base::ScopedCFTypeRef<CFStringRef> name) {
  return Launchd::GetInstance()->RestartJob(
      LaunchdDomain(), ServiceLaunchdType(), name, CFSTR("Aqua"));
}

bool StartUpdateAdministrationVersionedLaunchdJob(
    const base::ScopedCFTypeRef<CFStringRef> name) {
  return Launchd::GetInstance()->RestartJob(
      LaunchdDomain(), ServiceLaunchdType(), name, CFSTR("Aqua"));
}

bool StartLaunchdServiceJob() {
  return StartUpdateServiceVersionedLaunchdJob(
      CopyGoogleUpdateServiceLaunchDName());
}

bool RemoveJobFromLaunchd(Launchd::Domain domain,
                          Launchd::Type type,
                          base::ScopedCFTypeRef<CFStringRef> name) {
  // This may block while deleting the launchd plist file.
  base::ScopedBlockingCall scoped_blocking_call(FROM_HERE,
                                                base::BlockingType::MAY_BLOCK);
  Launchd::GetInstance()->RemoveJob(base::SysCFStringRefToUTF8(name));
  return Launchd::GetInstance()->DeletePlist(domain, type, name);
}

bool RemoveClientJobFromLaunchd(base::ScopedCFTypeRef<CFStringRef> name) {
  return RemoveJobFromLaunchd(LaunchdDomain(), ClientLaunchdType(), name);
}

bool RemoveServiceJobFromLaunchd(base::ScopedCFTypeRef<CFStringRef> name) {
  return RemoveJobFromLaunchd(LaunchdDomain(), ServiceLaunchdType(), name);
}

bool RemoveUpdateServiceJobFromLaunchd(
    base::ScopedCFTypeRef<CFStringRef> name) {
  return RemoveServiceJobFromLaunchd(name);
}

bool RemoveUpdateServiceJobFromLaunchd() {
  return RemoveUpdateServiceJobFromLaunchd(
      CopyGoogleUpdateServiceLaunchDName());
}

bool RemoveUpdateAdministrationJobFromLaunchd(
    base::ScopedCFTypeRef<CFStringRef> name) {
  return RemoveClientJobFromLaunchd(name);
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

}  // namespace

int InstallCandidate() {
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

  if (!CreateUpdateServiceLaunchdJobPlist(
          info_plist->GoogleUpdateServiceLaunchdNameVersioned(),
          updater_executable_path)) {
    return -2;
  }

  if (!CreateUpdateAdministrationLaunchdJobPlist(
          info_plist->GoogleUpdateAdministrationLaunchdNameVersioned(),
          updater_executable_path)) {
    return -3;
  }

  if (!StartUpdateServiceVersionedLaunchdJob(
          info_plist->GoogleUpdateServiceLaunchdNameVersioned())) {
    return -4;
  }

  if (!StartUpdateAdministrationVersionedLaunchdJob(
          info_plist->GoogleUpdateAdministrationLaunchdNameVersioned())) {
    return -5;
  }

  return 0;
}

int UninstallCandidate() {
  const std::unique_ptr<InfoPlist> info_plist =
      InfoPlist::Create(InfoPlistPath());
  CHECK(info_plist);

  RemoveUpdateAdministrationJobFromLaunchd(
      info_plist->GoogleUpdateAdministrationLaunchdNameVersioned());
  RemoveUpdateServiceJobFromLaunchd(
      info_plist->GoogleUpdateServiceLaunchdNameVersioned());
  DeleteInstallFolder(
      info_plist->UpdaterVersionedFolderPath(GetUpdaterFolderPath()));

  return 0;
}

int PromoteCandidate() {
  const std::unique_ptr<InfoPlist> info_plist =
      InfoPlist::Create(InfoPlistPath());
  CHECK(info_plist);

  const base::FilePath dest_path =
      info_plist->UpdaterVersionedFolderPath(GetUpdaterFolderPath());

  base::FilePath updater_executable_path = info_plist->UpdaterExecutablePath(
      GetLibraryFolderPath(), GetUpdateFolderName(), GetUpdaterAppName(),
      GetUpdaterAppExecutablePath());

  if (!CreateUpdateServiceLaunchdJobPlist(CopyGoogleUpdateServiceLaunchDName(),
                                          updater_executable_path)) {
    return -1;
  }

  if (!StartLaunchdServiceJob()) {
    return -2;
  }

  return 0;
}

#pragma mark Uninstall
int Uninstall(bool is_machine) {
  ALLOW_UNUSED_LOCAL(is_machine);

  const std::unique_ptr<InfoPlist> info_plist =
      InfoPlist::Create(InfoPlistPath());
  CHECK(info_plist);

  if (!RemoveUpdateServiceJobFromLaunchd(
          info_plist->GoogleUpdateServiceLaunchdNameVersioned())) {
    return -1;
  }

  if (!RemoveUpdateAdministrationJobFromLaunchd(
          info_plist->GoogleUpdateAdministrationLaunchdNameVersioned()))
    return -2;

  if (!RemoveUpdateServiceJobFromLaunchd())
    return -3;

  if (!DeleteInstallFolder())
    return -4;

  return 0;
}

}  // namespace updater
