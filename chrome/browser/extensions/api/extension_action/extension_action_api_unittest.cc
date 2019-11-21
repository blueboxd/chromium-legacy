// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/files/file_util.h"
#include "base/strings/stringprintf.h"
#include "chrome/browser/extensions/extension_service.h"
#include "chrome/browser/extensions/extension_service_test_with_install.h"
#include "chrome/common/extensions/api/extension_action/action_info.h"
#include "extensions/common/manifest_constants.h"
#include "extensions/test/test_extension_dir.h"

namespace extensions {
namespace {

enum class TestActionType {
  kBrowser,
  kPage,
};

class ExtensionActionAPIUnitTest
    : public ExtensionServiceTestWithInstall,
      public ::testing::WithParamInterface<TestActionType> {
 public:
  ExtensionActionAPIUnitTest() {}
  ~ExtensionActionAPIUnitTest() override {}

  const char* GetManifestKey() {
    switch (GetParam()) {
      case TestActionType::kBrowser:
        return manifest_keys::kBrowserAction;
      case TestActionType::kPage:
        return manifest_keys::kPageAction;
    }
    NOTREACHED();
    return nullptr;
  }

  const ActionInfo* GetActionInfo(const Extension& extension) {
    switch (GetParam()) {
      case TestActionType::kBrowser:
        return ActionInfo::GetBrowserActionInfo(&extension);
      case TestActionType::kPage:
        return ActionInfo::GetPageActionInfo(&extension);
    }
    NOTREACHED();
    return nullptr;
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(ExtensionActionAPIUnitTest);
};

// Test that extensions can provide icons of arbitrary sizes in the manifest.
TEST_P(ExtensionActionAPIUnitTest, MultiIcons) {
  InitializeEmptyExtensionService();

  constexpr char kManifestTemplate[] =
      R"({
           "name": "A test extension that tests multiple browser action icons",
           "version": "1.0",
           "manifest_version": 2,
           "%s": {
             "default_icon": {
               "19": "icon19.png",
               "24": "icon24.png",
               "31": "icon24.png",
               "38": "icon38.png"
             }
           }
         })";

  TestExtensionDir test_extension_dir;
  test_extension_dir.WriteManifest(
      base::StringPrintf(kManifestTemplate, GetManifestKey()));

  {
    std::string icon_file_content;
    base::FilePath icon_path = data_dir().AppendASCII("icon1.png");
    EXPECT_TRUE(base::ReadFileToString(icon_path, &icon_file_content));
    test_extension_dir.WriteFile(FILE_PATH_LITERAL("icon19.png"),
                                 icon_file_content);
    test_extension_dir.WriteFile(FILE_PATH_LITERAL("icon24.png"),
                                 icon_file_content);
    test_extension_dir.WriteFile(FILE_PATH_LITERAL("icon38.png"),
                                 icon_file_content);
  }

  const Extension* extension =
      PackAndInstallCRX(test_extension_dir.UnpackedPath(), INSTALL_NEW);
  EXPECT_TRUE(extension->install_warnings().empty());
  const ActionInfo* action_info = GetActionInfo(*extension);
  ASSERT_TRUE(action_info);

  const ExtensionIconSet& icons = action_info->default_icon;

  EXPECT_EQ(4u, icons.map().size());
  EXPECT_EQ("icon19.png", icons.Get(19, ExtensionIconSet::MATCH_EXACTLY));
  EXPECT_EQ("icon24.png", icons.Get(24, ExtensionIconSet::MATCH_EXACTLY));
  EXPECT_EQ("icon24.png", icons.Get(31, ExtensionIconSet::MATCH_EXACTLY));
  EXPECT_EQ("icon38.png", icons.Get(38, ExtensionIconSet::MATCH_EXACTLY));
}

INSTANTIATE_TEST_SUITE_P(All,
                         ExtensionActionAPIUnitTest,
                         testing::Values(TestActionType::kBrowser,
                                         TestActionType::kPage));

}  // namespace
}  // namespace extensions
