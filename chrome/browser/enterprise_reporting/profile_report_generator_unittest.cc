// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/enterprise_reporting/profile_report_generator.h"

#include "base/logging.h"
#include "base/run_loop.h"
#include "base/strings/string16.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/bind_test_util.h"
#include "chrome/browser/profiles/profile_attributes_storage.h"
#include "chrome/browser/signin/identity_test_environment_profile_adaptor.h"
#include "chrome/test/base/testing_browser_process.h"
#include "chrome/test/base/testing_profile_manager.h"
#include "components/account_id/account_id.h"
#include "components/signin/public/identity_manager/identity_test_environment.h"
#include "content/public/browser/plugin_service.h"
#include "content/public/common/webplugininfo.h"
#include "content/public/test/test_browser_thread_bundle.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace enterprise_reporting {
namespace {

const char kProfile[] = "Profile";
const char kIdleProfile[] = "IdleProfile";

const char kPluginName[] = "plugin";
const char kPluginVersion[] = "1.0";
const char kPluginDescription[] = "This is a plugin.";
const char kPluginFileName[] = "file_name";

}  // namespace

class ProfileReportGeneratorTest : public ::testing::Test {
 public:
  ProfileReportGeneratorTest()
      : profile_manager_(TestingBrowserProcess::GetGlobal()) {}
  ~ProfileReportGeneratorTest() override = default;

  void SetUp() override {
    ASSERT_TRUE(profile_manager_.SetUp());
    profile_ = profile_manager_.CreateTestingProfile(kProfile);
    content::PluginService::GetInstance()->Init();
  }

  std::unique_ptr<em::ChromeUserProfileInfo> GenerateReport(
      const base::FilePath& path,
      const std::string& name) {
    base::RunLoop run_loop;
    std::unique_ptr<em::ChromeUserProfileInfo> report =
        std::make_unique<em::ChromeUserProfileInfo>();
    generator_.MaybeGenerate(
        path, name,
        base::BindLambdaForTesting(
            [&run_loop, report = report.get()](
                std::unique_ptr<em::ChromeUserProfileInfo> response) {
              DCHECK(response);
              report->Swap(response.get());
              run_loop.Quit();
            }));
    run_loop.Run();
    return report;
  }

  std::unique_ptr<em::ChromeUserProfileInfo> GenerateReport() {
    auto report =
        GenerateReport(profile()->GetPath(), profile()->GetProfileUserName());
    EXPECT_TRUE(report);
    EXPECT_EQ(profile()->GetProfileUserName(), report->name());
    EXPECT_EQ(profile()->GetPath().AsUTF8Unsafe(), report->id());
    EXPECT_TRUE(report->is_full_report());

    return report;
  }

  void CreatePlugin() {
    content::WebPluginInfo info;
    info.name = base::ASCIIToUTF16(kPluginName);
    info.version = base::ASCIIToUTF16(kPluginVersion);
    info.desc = base::ASCIIToUTF16(kPluginDescription);
    info.path =
        base::FilePath().AppendASCII("path").AppendASCII(kPluginFileName);
    content::PluginService* plugin_service =
        content::PluginService::GetInstance();
    plugin_service->RegisterInternalPlugin(info, true);
    plugin_service->RefreshPlugins();
  }

  TestingProfile* profile() { return profile_; }
  TestingProfileManager* profile_manager() { return &profile_manager_; }

  ProfileReportGenerator generator_;

 private:
  content::TestBrowserThreadBundle thread_bundle_;
  TestingProfileManager profile_manager_;
  TestingProfile* profile_;

  DISALLOW_COPY_AND_ASSIGN(ProfileReportGeneratorTest);
};

TEST_F(ProfileReportGeneratorTest, ProfileNotActivated) {
  const base::FilePath profile_path =
      profile_manager()->profiles_dir().AppendASCII(kIdleProfile);
  profile_manager()->profile_attributes_storage()->AddProfile(
      profile_path, base::ASCIIToUTF16(kIdleProfile), std::string(),
      base::string16(), 0, std::string(), EmptyAccountId());
  base::RunLoop run_loop;
  generator_.MaybeGenerate(
      profile_path, kIdleProfile,
      base::BindLambdaForTesting(
          [&run_loop](std::unique_ptr<em::ChromeUserProfileInfo> response) {
            EXPECT_FALSE(response);
            run_loop.Quit();
          }));
  run_loop.Run();
}

TEST_F(ProfileReportGeneratorTest, UnsignedInProfile) {
  auto report = GenerateReport();
  EXPECT_FALSE(report->has_chrome_signed_in_user());
}

TEST_F(ProfileReportGeneratorTest, SignedInProfile) {
  IdentityTestEnvironmentProfileAdaptor identity_test_env_adaptor(profile());
  auto expected_info =
      identity_test_env_adaptor.identity_test_env()->SetPrimaryAccount(
          "test@mail.com");
  auto report = GenerateReport();
  EXPECT_TRUE(report->has_chrome_signed_in_user());
  EXPECT_EQ(expected_info.email, report->chrome_signed_in_user().email());
  EXPECT_EQ(expected_info.gaia,
            report->chrome_signed_in_user().obfudscated_gaiaid());
}

TEST_F(ProfileReportGeneratorTest, PluginIsDisabled) {
  CreatePlugin();
  generator_.set_extensions_and_plugins_enabled(false);
  auto report = GenerateReport();
  EXPECT_EQ(0, report->plugins_size());
}

TEST_F(ProfileReportGeneratorTest, PluginIsEnabled) {
  CreatePlugin();
  auto report = GenerateReport();
  // There might be other plugins like PDF plugin, however, our fake plugin
  // should be the first one in the report.
  EXPECT_LE(1, report->plugins_size());
  EXPECT_EQ(kPluginName, report->plugins(0).name());
  EXPECT_EQ(kPluginVersion, report->plugins(0).version());
  EXPECT_EQ(kPluginDescription, report->plugins(0).description());
  EXPECT_EQ(kPluginFileName, report->plugins(0).filename());
}

}  // namespace enterprise_reporting
