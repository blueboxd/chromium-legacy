// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/command_line.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/logging.h"
#include "base/path_service.h"
#include "base/process/launch.h"
#include "base/win/windows_types.h"
#include "chrome/browser/web_applications/chrome_pwa_launcher/last_browser_file_util.h"
#include "chrome/browser/web_applications/chrome_pwa_launcher/launcher_log.h"
#include "chrome/browser/web_applications/chrome_pwa_launcher/launcher_update.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/install_static/install_details.h"
#include "chrome/install_static/product_install_details.h"
#include "components/version_info/version_info_values.h"

namespace {

// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused.
enum LaunchResult {
  kSuccess = 0,
  kStarted = 1,
  kError = 2,
  kMaxValue = kStarted
};

// Returns the current executable's path, with capitalization preserved. If
// getting the current path fails, the launcher crashes.
base::FilePath GetCurrentExecutablePath() {
  base::FilePath current_path;
  CHECK(base::PathService::Get(base::FILE_EXE, &current_path));
  base::NormalizeFilePath(current_path, &current_path);
  return current_path;
}

// Returns the path to chrome.exe stored in the "Last Browser" file. If the file
// is not found, can't be read, or does not contain a valid path, the launcher
// crashes.
base::FilePath GetChromePathFromLastBrowserFile(
    const base::FilePath& current_path) {
  // The Last Browser file is expected to be in the User Data directory, which
  // is the great-grandparent of the current directory (User Data\<profile>\Web
  // Applications\<app ID>).
  const base::FilePath last_browser_file_path =
      web_app::GetLastBrowserFileFromWebAppDir(current_path.DirName());
  CHECK(base::PathExists(last_browser_file_path));

  // Get the path of chrome.exe stored in |last_browser_file_path|.
  const base::FilePath chrome_path =
      web_app::ReadChromePathFromLastBrowserFile(last_browser_file_path);
  CHECK(!chrome_path.empty());
  CHECK(base::PathExists(chrome_path));
  return chrome_path;
}

// Launches |chrome_path| with the current command-line arguments, and returns
// the launch result.
LaunchResult LaunchPwa(const base::FilePath& chrome_path) {
  // Launch chrome.exe, passing it all command-line arguments.
  base::CommandLine command_line(chrome_path);
  command_line.AppendArguments(*base::CommandLine::ForCurrentProcess(),
                               /*include_program=*/false);

  // Pass the current launcher version to chrome.exe. chrome.exe will update all
  // PWA launchers if an update is available.
  // NOTE: changing how the launcher version is passed to chrome.exe requires
  // adding legacy handling for the previous method, as older PWA launchers
  // still using this switch will rely on chrome.exe to update them to use the
  // new method.
  command_line.AppendSwitchASCII(switches::kPwaLauncherVersion,
                                 PRODUCT_VERSION);

  base::LaunchOptions launch_options;
  launch_options.current_directory = chrome_path.DirName();
  launch_options.grant_foreground_privilege = true;
  if (!base::LaunchProcess(command_line, launch_options).IsValid())
    return LaunchResult::kError;
  return LaunchResult::kSuccess;
}

}  // namespace

// This binary is a launcher for Progressive Web Apps. Each PWA has an
// individual hardlink or copy of chrome_pwa_launcher.exe in its web-app
// directory (User Data\<profile>\Web Applications\<app ID>), which allows the
// PWA to register as a file handler on Windows. chrome_pwa_launcher.exe assumes
// that it is run from a subdirectory of the User Data directory, and launches
// the chrome.exe that last used its containing User Data directory.
int WINAPI wWinMain(HINSTANCE instance,
                    HINSTANCE prev_instance,
                    wchar_t* /*command_line*/,
                    int show_command) {
  base::CommandLine::Init(0, nullptr);
  logging::LoggingSettings logging_settings;
  logging_settings.logging_dest = logging::LOG_TO_SYSTEM_DEBUG_LOG;
  logging::InitLogging(logging_settings);

  base::FilePath current_path = GetCurrentExecutablePath();
  base::FilePath chrome_path = GetChromePathFromLastBrowserFile(current_path);
  install_static::InstallDetails::SetForProcess(
      install_static::MakeProductDetails(chrome_path.value()));

  web_app::LauncherLog launcher_log;
  launcher_log.Log(LaunchResult::kStarted);

  auto launch_result = LaunchPwa(chrome_path);
  launcher_log.Log(launch_result);
  return launch_result;
}
