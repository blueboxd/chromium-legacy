// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_UPDATER_WIN_SETUP_SETUP_UTIL_H_
#define CHROME_UPDATER_WIN_SETUP_SETUP_UTIL_H_

#include <guiddef.h>

namespace base {
class CommandLine;
}  // namespace base

#include "base/strings/string16.h"
#include "base/win/windows_types.h"

namespace updater {

bool RegisterUpdateAppsTask(const base::CommandLine& run_command);
void UnregisterUpdateAppsTask();

base::string16 GetComServerClsid();
base::string16 GetComServerClsidRegistryPath();
base::string16 GetComServiceClsid();
base::string16 GetComServiceClsidRegistryPath();
base::string16 GetComServiceAppidRegistryPath();
base::string16 GetComIidRegistryPath(REFIID iid);
base::string16 GetComTypeLibRegistryPath(REFIID iid);

}  // namespace updater

#endif  // CHROME_UPDATER_WIN_SETUP_SETUP_UTIL_H_
