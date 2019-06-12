// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/updater/updater_constants.h"

namespace updater {

const char kCrashMeSwitch[] = "crash-me";
const char kCrashHandlerSwitch[] = "crash-handler";
const char kInstall[] = "install";
const char kUninstall[] = "uninstall";
const char kTestSwitch[] = "test";

const char kNoRateLimit[] = "--no-rate-limit";

const char kUpdaterJSONDefaultUrl[] =
    "https://update.googleapis.com/service/update2/json";
const char kCrashUploadURL[] = "https://clients2.google.com/cr/report";
const char kCrashStagingUploadURL[] =
    "https://clients2.google.com/cr/staging_report";

extern const char kAppsDir[] = "apps";
extern const char kUninstallScript[] = "uninstall.cmd";

}  // namespace updater
