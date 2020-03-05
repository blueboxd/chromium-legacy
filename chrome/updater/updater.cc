// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/updater/updater.h"

#include "base/at_exit.h"
#include "base/command_line.h"
#include "base/files/file_path.h"
#include "base/logging.h"
#include "build/build_config.h"
#include "chrome/updater/app/app.h"
#include "chrome/updater/app/app_uninstall.h"
#include "chrome/updater/app/app_update_all.h"
#include "chrome/updater/configurator.h"
#include "chrome/updater/constants.h"
#include "chrome/updater/crash_client.h"
#include "chrome/updater/crash_reporter.h"
#include "chrome/updater/util.h"

#if defined(OS_WIN)
#include "chrome/updater/server/win/server.h"
#include "chrome/updater/server/win/service_main.h"
#include "chrome/updater/win/install_app.h"
#endif

#if defined(OS_MACOSX)
#include "chrome/updater/server/mac/server.h"
#endif

// To install the updater on Windows, run "updatersetup.exe" from the
// build directory.
//
// To uninstall, run "updater.exe --uninstall" from its install directory,
// which is under %LOCALAPPDATA%\Google\GoogleUpdater, or from the |out|
// directory of the build.
//
// To debug, use the command line arguments:
//    --enable-logging --vmodule=*/chrome/updater/*=2.

namespace updater {

namespace {

// The log file is created in DIR_LOCAL_APP_DATA or DIR_APP_DATA.
void InitLogging(const base::CommandLine& command_line) {
  logging::LoggingSettings settings;
  base::FilePath log_dir;
  GetProductDirectory(&log_dir);
  const auto log_file = log_dir.Append(FILE_PATH_LITERAL("updater.log"));
  settings.log_file_path = log_file.value().c_str();
  settings.logging_dest = logging::LOG_TO_ALL;
  logging::InitLogging(settings);
  logging::SetLogItems(true,    // enable_process_id
                       true,    // enable_thread_id
                       true,    // enable_timestamp
                       false);  // enable_tickcount
  VLOG(1) << "Log file " << settings.log_file_path;
}

}  // namespace

int HandleUpdaterCommands(const base::CommandLine* command_line) {
  DCHECK(!command_line->HasSwitch(kCrashHandlerSwitch));

  if (command_line->HasSwitch(kCrashMeSwitch)) {
    int* ptr = nullptr;
    return *ptr;
  }

  if (command_line->HasSwitch(kServerSwitch)) {
    return MakeAppServer()->Run();
  }

#if defined(OS_WIN)
  if (command_line->HasSwitch(kComServiceSwitch))
    return ServiceMain::RunComService(command_line);

  if (command_line->HasSwitch(kInstallSwitch))
    return MakeAppInstall({kChromeAppId})->Run();
#endif

  if (command_line->HasSwitch(kUninstallSwitch))
    return MakeAppUninstall()->Run();

  if (command_line->HasSwitch(kUpdateAppsSwitch)) {
    return MakeAppUpdateAll()->Run();
  }

  VLOG(1) << "Unknown command line switch.";
  return -1;
}

int UpdaterMain(int argc, const char* const* argv) {
  base::PlatformThread::SetName("UpdaterMain");
  base::AtExitManager exit_manager;

  base::CommandLine::Init(argc, argv);
  const auto* command_line = base::CommandLine::ForCurrentProcess();
  if (command_line->HasSwitch(kTestSwitch))
    return 0;

  InitLogging(*command_line);

  if (command_line->HasSwitch(kCrashHandlerSwitch))
    return CrashReporterMain();

  return HandleUpdaterCommands(command_line);
}

}  // namespace updater
