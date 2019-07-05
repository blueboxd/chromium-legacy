// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/enterprise_reporting/extension_info.h"

#include "chrome/browser/enterprise_reporting/profile_report_generator.h"
#include "chrome/browser/extensions/extension_service.h"
#include "chrome/browser/extensions/extension_service_test_base.h"
#include "chrome/test/base/testing_profile.h"
#include "extensions/common/extension_builder.h"
#include "extensions/common/manifest_constants.h"
#include "extensions/common/value_builder.h"

namespace em = enterprise_management;

namespace enterprise_reporting {
namespace {

const char kName[] = "extension";
const char kId[] = "abcdefghijklmnoabcdefghijklmnoab";
const char kId2[] = "abcdefghijklmnoabcdefghijklmnoac";
const char kVersion[] = "1.0.0";
const char kDescription[] = "an extension description.";
const char kHomepage[] = "https://foo.com/extension";
const char kPermission1[] = "alarms";
const char kPermission2[] = "idle";
const char kPermission3[] = "*://*.example.com/*";

}  // namespace

class ExtensionInfoTest : public extensions::ExtensionServiceTestBase {
 public:
  void SetUp() override {
    extensions::ExtensionServiceTestBase::SetUp();
    InitializeEmptyExtensionService();
    service()->Init();
  }

  scoped_refptr<const extensions::Extension> BuildExtension() {
    return BuildExtension(kId, extensions::Manifest::UNPACKED);
  }
  scoped_refptr<const extensions::Extension> BuildExtension(
      const std::string& id,
      extensions::Manifest::Location location) {
    auto extension =
        extensions::ExtensionBuilder(kName)
            .SetID(id)
            .SetVersion(kVersion)
            .SetManifestKey(extensions::manifest_keys::kDescription,
                            kDescription)
            .SetManifestKey(extensions::manifest_keys::kHomepageURL, kHomepage)
            .SetLocation(location)
            .AddPermission(kPermission1)
            .AddPermission(kPermission2)
            .AddPermission(kPermission3)
            .Build();
    service()->AddExtension(extension.get());
    return extension;
  }
};

TEST_F(ExtensionInfoTest, ExtensionReport) {
  auto extension = BuildExtension();

  em::ChromeUserProfileInfo info;
  AppendExtensionInfoIntoProfileReport(profile(), &info);

  EXPECT_EQ(1, info.extensions_size());
  const em::Extension actual_extension_report = info.extensions(0);

  EXPECT_EQ(kId, actual_extension_report.id());
  EXPECT_EQ(kName, actual_extension_report.name());
  EXPECT_EQ(kVersion, actual_extension_report.version());
  EXPECT_EQ(kDescription, actual_extension_report.description());

  EXPECT_EQ(em::Extension_ExtensionType_TYPE_EXTENSION,
            actual_extension_report.app_type());
  EXPECT_EQ(em::Extension_InstallType_TYPE_DEVELOPMENT,
            actual_extension_report.install_type());

  EXPECT_EQ(kHomepage, actual_extension_report.homepage_url());

  EXPECT_TRUE(actual_extension_report.enabled());

  EXPECT_EQ(2, actual_extension_report.permissions_size());
  EXPECT_EQ(kPermission1, actual_extension_report.permissions(0));
  EXPECT_EQ(kPermission2, actual_extension_report.permissions(1));
  EXPECT_EQ(1, actual_extension_report.host_permissions_size());
  EXPECT_EQ(kPermission3, actual_extension_report.host_permissions(0));
}

TEST_F(ExtensionInfoTest, MultipleExtensions) {
  auto extension1 = BuildExtension(kId, extensions::Manifest::UNPACKED);
  auto extension2 = BuildExtension(kId2, extensions::Manifest::UNPACKED);

  em::ChromeUserProfileInfo info;
  AppendExtensionInfoIntoProfileReport(profile(), &info);

  EXPECT_EQ(2, info.extensions_size());
  EXPECT_EQ(kId, info.extensions(0).id());
  EXPECT_EQ(kId2, info.extensions(1).id());
}

TEST_F(ExtensionInfoTest, ExtensionDisabled) {
  auto extension = BuildExtension();
  service()->DisableExtension(kId,
                              extensions::disable_reason::DISABLE_USER_ACTION);

  em::ChromeUserProfileInfo info;
  AppendExtensionInfoIntoProfileReport(profile(), &info);

  EXPECT_EQ(1, info.extensions_size());

  EXPECT_FALSE(info.extensions(0).enabled());
}

TEST_F(ExtensionInfoTest, ExtensionTerminated) {
  auto extension = BuildExtension();
  service()->TerminateExtension(kId);

  em::ChromeUserProfileInfo info;
  AppendExtensionInfoIntoProfileReport(profile(), &info);

  EXPECT_EQ(1, info.extensions_size());

  EXPECT_FALSE(info.extensions(0).enabled());
}

TEST_F(ExtensionInfoTest, ExtensionBlocked) {
  auto extension = BuildExtension();
  service()->BlockAllExtensions();

  em::ChromeUserProfileInfo info;
  AppendExtensionInfoIntoProfileReport(profile(), &info);

  EXPECT_EQ(0, info.extensions_size());
}

TEST_F(ExtensionInfoTest, ExtensionBlacklisted) {
  auto extension = BuildExtension();
  service()->BlacklistExtensionForTest(kId);

  em::ChromeUserProfileInfo info;
  AppendExtensionInfoIntoProfileReport(profile(), &info);

  EXPECT_EQ(0, info.extensions_size());
}

TEST_F(ExtensionInfoTest, ComponentExtension) {
  auto extension1 = BuildExtension(kId, extensions::Manifest::COMPONENT);
  auto extension2 =
      BuildExtension(kId2, extensions::Manifest::EXTERNAL_COMPONENT);

  em::ChromeUserProfileInfo info;
  AppendExtensionInfoIntoProfileReport(profile(), &info);

  EXPECT_EQ(0, info.extensions_size());
}

}  // namespace enterprise_reporting
