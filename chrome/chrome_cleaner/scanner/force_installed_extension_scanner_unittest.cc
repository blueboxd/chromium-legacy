// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/chrome_cleaner/scanner/force_installed_extension_scanner.h"

#include "base/base_paths_win.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/path_service.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/scoped_path_override.h"
#include "base/test/test_reg_util_win.h"
#include "base/win/registry.h"
#include "chrome/chrome_cleaner/chrome_utils/extension_id.h"
#include "chrome/chrome_cleaner/os/registry_util.h"
#include "chrome/chrome_cleaner/parsers/json_parser/test_json_parser.h"
#include "chrome/chrome_cleaner/pup_data/test_uws.h"
#include "chrome/chrome_cleaner/scanner/force_installed_extension_scanner_impl.h"
#include "chrome/chrome_cleaner/test/test_extensions.h"
#include "chrome/chrome_cleaner/test/test_file_util.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace chrome_cleaner {

using testing::Contains;

TEST(UwEMatcherTest, GetForceInstalledExtensions) {
  ForceInstalledExtensionScannerImpl extension_scanner;
  std::vector<ForceInstalledExtension> expected_extensions{
      {ExtensionID::Create(base::WideToUTF8(kTestExtensionId1)).value(),
       DEFAULT_APPS_EXTENSION},
      {ExtensionID::Create(base::WideToUTF8(kTestExtensionId2)).value(),
       DEFAULT_APPS_EXTENSION},
      {ExtensionID::Create(base::WideToUTF8(kTestExtensionId5)).value(),
       POLICY_EXTENSION_SETTINGS},
      {ExtensionID::Create(base::WideToUTF8(kTestExtensionId4)).value(),
       POLICY_EXTENSION_SETTINGS},
      {ExtensionID::Create(base::WideToUTF8(kTestExtensionId3)).value(),
       POLICY_EXTENSION_FORCELIST},
      {ExtensionID::Create(base::WideToUTF8(kTestExtensionId6)).value(),
       POLICY_MASTER_PREFERENCES},
      {ExtensionID::Create(base::WideToUTF8(kTestExtensionId7)).value(),
       POLICY_MASTER_PREFERENCES},
  };
  TestJsonParser json_parser;
  registry_util::RegistryOverrideManager registry_override;
  registry_override.OverrideRegistry(HKEY_LOCAL_MACHINE);
  registry_override.OverrideRegistry(HKEY_CURRENT_USER);

  // Add the force list registry entry
  for (const TestRegistryEntry& policy : kExtensionForcelistEntries) {
    base::win::RegKey policy_key;
    ASSERT_EQ(ERROR_SUCCESS, policy_key.Create(HKEY_LOCAL_MACHINE,
                                               kChromePoliciesForcelistKeyPath,
                                               KEY_ALL_ACCESS));
    ASSERT_TRUE(policy_key.Valid());
    ASSERT_EQ(ERROR_SUCCESS,
              policy_key.WriteValue(policy.name.c_str(), policy.value.c_str()));
  }

  // Add the UwE to the extension settings registry entry
  base::win::RegKey settings_key;
  ASSERT_EQ(ERROR_SUCCESS,
            settings_key.Create(HKEY_LOCAL_MACHINE,
                                kExtensionSettingsPolicyPath, KEY_ALL_ACCESS));
  ASSERT_TRUE(settings_key.Valid());
  ASSERT_EQ(ERROR_SUCCESS,
            settings_key.WriteValue(kExtensionSettingsName,
                                    kExtensionSettingsJsonOnlyForced));

  // Set up a fake default extensions JSON file.
  base::ScopedPathOverride program_files_override(base::DIR_PROGRAM_FILES);
  base::FilePath program_files_dir;
  ASSERT_TRUE(
      base::PathService::Get(base::DIR_PROGRAM_FILES, &program_files_dir));
  base::FilePath fake_apps_dir(
      program_files_dir.Append(kFakeChromeFolder).Append(L"default_apps"));
  ASSERT_TRUE(base::CreateDirectoryAndGetError(fake_apps_dir, nullptr));
  base::FilePath default_extensions_file =
      fake_apps_dir.Append(L"external_extensions.json");
  CreateFileWithContent(default_extensions_file, kDefaultExtensionsJson,
                        sizeof(kDefaultExtensionsJson) - 1);
  ASSERT_TRUE(base::PathExists(default_extensions_file));

  // Add the fake default extensions file.
  base::FilePath chrome_dir(program_files_dir.Append(kChromeExePath));
  ASSERT_TRUE(base::CreateDirectoryAndGetError(chrome_dir, nullptr));
  base::FilePath master_preferences =
      chrome_dir.Append(kMasterPreferencesFileName);
  CreateFileWithContent(master_preferences, kMasterPreferencesJson,
                        sizeof(kMasterPreferencesJson) - 1);
  ASSERT_TRUE(base::PathExists(master_preferences));

  std::vector<ForceInstalledExtension> found_extensions =
      extension_scanner.GetForceInstalledExtensions(&json_parser);
  EXPECT_THAT(found_extensions,
              testing::UnorderedElementsAreArray(expected_extensions));
}

TEST(UwEMatcherTest, GetNoForcedInstalledExtensions) {
  ForceInstalledExtensionScannerImpl extension_scanner;
  registry_util::RegistryOverrideManager registry_override;
  registry_override.OverrideRegistry(HKEY_LOCAL_MACHINE);
  registry_override.OverrideRegistry(HKEY_CURRENT_USER);

  // Add the force list registry entry
  base::win::RegKey policy_key;
  ASSERT_EQ(ERROR_SUCCESS,
            policy_key.Create(HKEY_LOCAL_MACHINE,
                              kChromePoliciesForcelistKeyPath, KEY_ALL_ACCESS));
  ASSERT_TRUE(policy_key.Valid());

  // Add the UwE to the extension settings registry entry
  base::win::RegKey settings_key;
  ASSERT_EQ(ERROR_SUCCESS,
            settings_key.Create(HKEY_LOCAL_MACHINE,
                                kExtensionSettingsPolicyPath, KEY_ALL_ACCESS));
  ASSERT_TRUE(settings_key.Valid());

  // Set up a fake default extensions JSON file.
  base::ScopedPathOverride program_files_override(base::DIR_PROGRAM_FILES);
  base::FilePath program_files_dir;
  ASSERT_TRUE(
      base::PathService::Get(base::DIR_PROGRAM_FILES, &program_files_dir));
  base::FilePath fake_apps_dir(
      program_files_dir.Append(kFakeChromeFolder).Append(L"default_apps"));
  ASSERT_TRUE(base::CreateDirectoryAndGetError(fake_apps_dir, nullptr));
  base::FilePath default_extensions_file =
      fake_apps_dir.Append(L"external_extensions.json");
  CreateEmptyFile(default_extensions_file);
  ASSERT_TRUE(base::PathExists(default_extensions_file));

  // Add the fake default extensions file.
  base::FilePath chrome_dir(program_files_dir.Append(kChromeExePath));
  ASSERT_TRUE(base::CreateDirectoryAndGetError(chrome_dir, nullptr));
  base::FilePath master_preferences =
      chrome_dir.Append(kMasterPreferencesFileName);
  CreateEmptyFile(master_preferences);
  ASSERT_TRUE(base::PathExists(master_preferences));

  TestJsonParser json_parser;
  std::vector<ForceInstalledExtension> extensions =
      extension_scanner.GetForceInstalledExtensions(&json_parser);
  ASSERT_EQ(extensions, std::vector<ForceInstalledExtension>{});
}

}  // namespace chrome_cleaner
