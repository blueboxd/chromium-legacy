// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_UPDATER_TEST_TEST_APP_CONSTANTS_H_
#define CHROME_UPDATER_TEST_TEST_APP_CONSTANTS_H_

namespace updater {

// Installs the updater.
extern const char kInstallUpdaterSwitch[];

// Registers the test app to the updater through IPC.
extern const char kRegisterToUpdaterSwitch[];

// Initiates a foreground update through IPC.
extern const char kForegroundUpdateSwitch[];

}  // namespace updater

#endif  // CHROME_UPDATER_TEST_TEST_APP_CONSTANTS_H_
