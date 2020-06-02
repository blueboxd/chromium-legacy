// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <windows.h>

#include "base/strings/string16.h"
#include "base/strings/utf_string_conversions.h"
#include "base/win/registry.h"
#include "chrome/updater/win/constants.h"
#include "chrome/updater/win/util.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace updater {

namespace {

bool SetInstallerProgress(const std::string& app_id, int value) {
  base::string16 subkey;
  if (!base::UTF8ToUTF16(app_id.c_str(), app_id.size(), &subkey)) {
    return false;
  }
  constexpr REGSAM kRegSam = KEY_WRITE | KEY_WOW64_32KEY;
  base::win::RegKey key(HKEY_CURRENT_USER,
                        base::ASCIIToUTF16(CLIENT_STATE_KEY).c_str(), kRegSam);
  if (key.CreateKey(subkey.c_str(), kRegSam) != ERROR_SUCCESS) {
    return false;
  }

  return key.WriteValue(kRegistryValueInstallerProgress, DWORD{value}) ==
         ERROR_SUCCESS;
}

bool DeleteKey(const std::string& app_id) {
  base::string16 subkey;
  if (!base::UTF8ToUTF16(app_id.c_str(), app_id.size(), &subkey)) {
    return false;
  }
  constexpr REGSAM kRegSam = KEY_WRITE | KEY_WOW64_32KEY;
  base::win::RegKey key(HKEY_CURRENT_USER,
                        base::ASCIIToUTF16(CLIENT_STATE_KEY).c_str(), kRegSam);
  return key.DeleteKey(subkey.c_str()) == ERROR_SUCCESS;
}

}  // namespace

TEST(UpdaterTestUtil, HRESULTFromLastError) {
  ::SetLastError(ERROR_ACCESS_DENIED);
  EXPECT_EQ(E_ACCESSDENIED, HRESULTFromLastError());
  ::SetLastError(ERROR_SUCCESS);
  EXPECT_EQ(E_FAIL, HRESULTFromLastError());
}

TEST(UpdaterTestUtil, GetDownloadProgress) {
  EXPECT_EQ(GetDownloadProgress(0, 50), 0);
  EXPECT_EQ(GetDownloadProgress(12, 50), 24);
  EXPECT_EQ(GetDownloadProgress(25, 50), 50);
  EXPECT_EQ(GetDownloadProgress(50, 50), 100);
  EXPECT_EQ(GetDownloadProgress(50, 50), 100);
  EXPECT_EQ(GetDownloadProgress(0, -1), -1);
  EXPECT_EQ(GetDownloadProgress(-1, -1), -1);
  EXPECT_EQ(GetDownloadProgress(50, 0), -1);
}

TEST(UpdaterTestUtil, InstallerProgress) {
  constexpr char kAppId[] = "{55d6c27c-8b97-4b76-a691-2df8810004ed}";
  DeleteKey(kAppId);
  EXPECT_EQ(GetInstallerProgress(kAppId), -1);
  SetInstallerProgress(kAppId, 0);
  EXPECT_EQ(GetInstallerProgress(kAppId), 0);
  SetInstallerProgress(kAppId, 50);
  EXPECT_EQ(GetInstallerProgress(kAppId), 50);
  SetInstallerProgress(kAppId, 100);
  EXPECT_EQ(GetInstallerProgress(kAppId), 100);
  SetInstallerProgress(kAppId, 200);
  EXPECT_EQ(GetInstallerProgress(kAppId), 100);
  EXPECT_TRUE(DeleteKey(kAppId));
}

}  // namespace updater
