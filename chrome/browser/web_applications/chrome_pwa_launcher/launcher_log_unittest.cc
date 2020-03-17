// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/web_applications/chrome_pwa_launcher/launcher_log.h"

#include "base/test/test_reg_util_win.h"
#include "chrome/install_static/install_util.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

constexpr wchar_t kValueName[] = L"PWALauncherResult";

}  // namespace

namespace web_app {

TEST(WebAppLauncherTest, LauncherLog) {
  registry_util::RegistryOverrideManager registry_override;
  ASSERT_NO_FATAL_FAILURE(
      registry_override.OverrideRegistry(HKEY_CURRENT_USER));

  base::win::RegKey key(HKEY_CURRENT_USER,
                        install_static::GetRegistryPath().c_str(),
                        KEY_CREATE_SUB_KEY | KEY_READ);
  ASSERT_TRUE(key.Valid());

  const DWORD expected_value = 5U;

  LauncherLog launcher_log;
  ASSERT_FALSE(key.HasValue(kValueName));
  launcher_log.Log(expected_value);
  ASSERT_TRUE(key.HasValue(kValueName));

  DWORD logged_value;
  ASSERT_EQ(ERROR_SUCCESS, key.ReadValueDW(kValueName, &logged_value));
  ASSERT_EQ(expected_value, logged_value);
}

}  // namespace web_app
