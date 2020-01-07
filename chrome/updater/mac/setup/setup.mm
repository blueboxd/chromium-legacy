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
#include "base/mac/foundation_util.h"
#include "base/mac/scoped_nsobject.h"
#include "base/path_service.h"
#include "base/strings/sys_string_conversions.h"
#include "base/task/thread_pool/thread_pool_instance.h"
#include "base/threading/scoped_blocking_call.h"
#include "build/build_config.h"
#include "chrome/common/mac/launchd.h"
#include "chrome/updater/crash_client.h"
#include "chrome/updater/crash_reporter.h"
#include "chrome/updater/updater_constants.h"
#include "chrome/updater/updater_version.h"
#include "chrome/updater/util.h"
#include "components/crash/core/common/crash_key.h"

namespace updater {

namespace setup {

namespace {

constexpr base::FilePath::CharType kUpdaterFolder[] =
    FILE_PATH_LITERAL("Google/GoogleUpdate/");

void ThreadPoolStart() {
  base::ThreadPoolInstance::CreateAndStartWithDefaultParams("UpdaterSetup");
}

void ThreadPoolStop() {
  base::ThreadPoolInstance::Get()->Shutdown();
}

// The log file is created in DIR_LOCAL_APP_DATA or DIR_APP_DATA.
void InitLogging(const base::CommandLine& command_line) {
  logging::LoggingSettings settings;
  base::FilePath log_dir;
  updater::GetProductDirectory(&log_dir);
  const auto log_file = log_dir.Append(FILE_PATH_LITERAL("updater_setup.log"));
  settings.log_file_path = log_file.value().c_str();
  settings.logging_dest = logging::LOG_TO_ALL;
  logging::InitLogging(settings);
  logging::SetLogItems(true,    // enable_process_id
                       true,    // enable_thread_id
                       true,    // enable_timestamp
                       false);  // enable_tickcount
  VLOG(1) << "Log file " << settings.log_file_path;
}

void InitializeUpdaterSetupMain() {
  crash_reporter::InitializeCrashKeys();

  static crash_reporter::CrashKeyString<16> crash_key_process_type(
      "process_type");
  crash_key_process_type.Set("updater_setup");

  if (updater::CrashClient::GetInstance()->InitializeCrashReporting())
    VLOG(1) << "Crash reporting initialized.";
  else
    VLOG(1) << "Crash reporting is not available.";

  updater::StartCrashReporter(UPDATER_VERSION_STRING);

  ThreadPoolStart();
}

void TerminateUpdaterSetupMain() {
  ThreadPoolStop();
}

bool CopyBundle() {
  // Copy bundle to "~/Library/Google/GoogleUpdate".
  const base::FilePath dest_path =
      base::mac::GetUserLibraryPath().Append(kUpdaterFolder);

  base::FilePath this_executable_path;
  base::PathService::Get(base::FILE_EXE, &this_executable_path);
  const base::FilePath src_path = this_executable_path.DirName().Append(
      FILE_PATH_LITERAL("GoogleUpdate.app"));

  if (!base::CopyDirectory(src_path, dest_path, true)) {
    LOG(ERROR) << "Copying app to ~/Library failed";
    return false;
  }
  return true;
}

bool DeleteInstallFolder() {
  // Delete the install folder - "~/Library/Google/GoogleUpdate".
  const base::FilePath dest_path =
      base::mac::GetUserLibraryPath().Append(kUpdaterFolder);

  if (!base::DeleteFileRecursively(dest_path)) {
    LOG(ERROR) << "Deleting " << dest_path << " failed";
    return false;
  }
  return true;
}

base::ScopedCFTypeRef<CFStringRef> CopyGoogleUpdateCheckLaunchDName() {
  return base::ScopedCFTypeRef<CFStringRef>(CFStringCreateCopy(
      kCFAllocatorDefault, CFSTR("com.google.GoogleUpdate.check")));
}

base::scoped_nsobject<NSString> GetGoogleUpdateCheckLaunchDLabel() {
  base::scoped_nsobject<NSString> label(
      base::mac::CFToNSCast(CopyGoogleUpdateCheckLaunchDName()));
  return label;
}

base::scoped_nsobject<NSString> GetGoogleUpdateCheckMachName() {
  base::scoped_nsobject<NSString> name(
      base::mac::CFToNSCast(CopyGoogleUpdateCheckLaunchDName()));
  return base::scoped_nsobject<NSString>(
      [name stringByAppendingFormat:@".%lu",
                                    [GetGoogleUpdateCheckLaunchDLabel() hash]],
      base::scoped_policy::RETAIN);
}

base::ScopedCFTypeRef<CFDictionaryRef> CreateGoogleUpdateCheckLaunchdPlist(
    base::FilePath* updater_path) {
  // See the man page for launchd.plist.
  NSDictionary* launchd_plist = @{
    @LAUNCH_JOBKEY_LABEL : GetGoogleUpdateCheckLaunchDLabel(),
    @LAUNCH_JOBKEY_PROGRAM : base::SysUTF8ToNSString(updater_path->value()),
    @LAUNCH_JOBKEY_PROGRAMARGUMENTS : @[ @"--ua" ],
    @LAUNCH_JOBKEY_MACHSERVICES : GetGoogleUpdateCheckMachName(),
    @LAUNCH_JOBKEY_RUNATLOAD : @YES,
    @LAUNCH_JOBKEY_STARTINTERVAL : @18000,
    @LAUNCH_JOBKEY_KEEPALIVE : @{@LAUNCH_JOBKEY_KEEPALIVE_SUCCESSFULEXIT : @NO},
    @LAUNCH_JOBKEY_LIMITLOADTOSESSIONTYPE : @"Aqua"
  };

  return base::ScopedCFTypeRef<CFDictionaryRef>(
      base::mac::CFCast<CFDictionaryRef>(launchd_plist),
      base::scoped_policy::RETAIN);
}

bool CreateLaunchdItems() {
  // We're creating directories and writing a file.
  base::ScopedBlockingCall scoped_blocking_call(FROM_HERE,
                                                base::BlockingType::MAY_BLOCK);
  base::ScopedCFTypeRef<CFStringRef> name(CopyGoogleUpdateCheckLaunchDName());

  base::FilePath updater_path =
      base::mac::GetUserLibraryPath()
          .Append(kUpdaterFolder)
          .Append(FILE_PATH_LITERAL("GoogleUpdate.app"))
          .Append(FILE_PATH_LITERAL("Contents/MacOS/GoogleUpdate"));

  base::ScopedCFTypeRef<CFDictionaryRef> plist(
      CreateGoogleUpdateCheckLaunchdPlist(&updater_path));
  return Launchd::GetInstance()->WritePlistToFile(Launchd::User, Launchd::Agent,
                                                  name, plist);
}

bool RemoveFromLaunchd() {
  // This may block while deleting the launchd plist file.
  base::ScopedBlockingCall scoped_blocking_call(FROM_HERE,
                                                base::BlockingType::MAY_BLOCK);
  base::ScopedCFTypeRef<CFStringRef> name(CopyGoogleUpdateCheckLaunchDName());
  return Launchd::GetInstance()->DeletePlist(Launchd::User, Launchd::Agent,
                                             name);
}

int SetupUpdater() {
  if (!CopyBundle())
    return -1;

  if (!CreateLaunchdItems())
    return -2;

  return 0;
}

}  // namespace

int HandleUpdaterSetupCommands(const base::CommandLine* command_line) {
  DCHECK(!command_line->HasSwitch(updater::kCrashHandlerSwitch));

  if (command_line->HasSwitch(updater::kCrashMeSwitch)) {
    LOG(FATAL) << "Crashing deliberately.";
    return -3;
  }

  return SetupUpdater();
}

int UpdaterSetupMain(int argc, const char* const* argv) {
  base::PlatformThread::SetName("UpdaterSetupMain");
  base::AtExitManager exit_manager;

  base::CommandLine::Init(argc, argv);
  const auto* command_line = base::CommandLine::ForCurrentProcess();
  if (command_line->HasSwitch(updater::kTestSwitch))
    return 0;

  InitLogging(*command_line);

  if (command_line->HasSwitch(updater::kCrashHandlerSwitch))
    return updater::CrashReporterMain();

  InitializeUpdaterSetupMain();
  const auto result = HandleUpdaterSetupCommands(command_line);
  TerminateUpdaterSetupMain();
  return result;
}

}  // namespace setup

int Uninstall() {
  if (!setup::RemoveFromLaunchd())
    return -1;

  if (!setup::DeleteInstallFolder())
    return -2;

  return 0;
}

}  // namespace updater
