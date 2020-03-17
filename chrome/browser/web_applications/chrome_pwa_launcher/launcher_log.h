// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_WEB_APPLICATIONS_CHROME_PWA_LAUNCHER_LAUNCHER_LOG_H_
#define CHROME_BROWSER_WEB_APPLICATIONS_CHROME_PWA_LAUNCHER_LAUNCHER_LOG_H_

#include "base/win/registry.h"

namespace web_app {

// A LauncherLog object provides the Log() function, which writes the given
// value to registry key HKCU\Software\
// [kCompanyPathName\]kProductPathName[install_suffix]:PWALauncherResult.
// LauncherLog is used by chrome_pwa_launcher.exe, which writes its last result
// code to the registry to provide insight into potential launcher issues
// without the overhead of full Crashpad integration.
// TODO(jessemckenna): read the latest value logged by each LauncherLog on
// startup and generate histograms.
class LauncherLog {
 public:
  LauncherLog();
  LauncherLog(const LauncherLog&) = delete;
  LauncherLog& operator=(const LauncherLog&) = delete;
  ~LauncherLog() = default;

  // Write |value| to PWALauncherResult in the registry.
  void Log(int value);

 private:
  base::win::RegKey key_;
};

}  // namespace web_app

#endif  // CHROME_BROWSER_WEB_APPLICATIONS_CHROME_PWA_LAUNCHER_LAUNCHER_LOG_H_
