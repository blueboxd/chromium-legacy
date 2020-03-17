// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/web_applications/chrome_pwa_launcher/launcher_log.h"

#include "base/numerics/safe_conversions.h"
#include "chrome/install_static/install_details.h"
#include "chrome/install_static/install_util.h"

namespace {

constexpr wchar_t kValueName[] = L"PWALauncherResult";

}  // namespace

namespace web_app {

LauncherLog::LauncherLog()
    : key_(HKEY_CURRENT_USER,
           install_static::GetRegistryPath().c_str(),
           KEY_SET_VALUE) {}

void LauncherLog::Log(int value) {
  key_.WriteValue(kValueName, base::saturated_cast<DWORD>(value));
}

}  // namespace web_app
