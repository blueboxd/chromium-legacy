// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/enterprise_reporting/profile_report_generator.h"

#include "base/logging.h"
#include "base/strings/string16.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/profiles/profile_attributes_storage.h"
#include "chrome/browser/signin/identity_test_environment_profile_adaptor.h"
#include "chrome/test/base/testing_browser_process.h"
#include "chrome/test/base/testing_profile_manager.h"
#include "components/account_id/account_id.h"
#include "content/public/test/test_browser_thread_bundle.h"
#include "services/identity/public/cpp/identity_test_environment.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace enterprise_reporting {
namespace {
const char kProfile[] = "Profile";
const char kIdleProfile[] = "IdleProfile";
}  // namespace
class ProfileReportGeneratorTest : public ::testing::Test {
 public:
  ProfileReportGeneratorTest()
      : profile_manager_(TestingBrowserProcess::GetGlobal()) {}
  ~ProfileReportGeneratorTest() override = default;

  void SetUp() override {
    ASSERT_TRUE(profile_manager_.SetUp());
    profile_ = profile_manager_.CreateTestingProfile(kProfile);
  }

  std::unique_ptr<em::ChromeUserProfileInfo> GenerateReport() {
    auto report = generator_.MaybeGenerate(profile()->GetPath(),
                                           profile()->GetProfileUserName());

    EXPECT_TRUE(report);
    EXPECT_EQ(profile()->GetProfileUserName(), report->name());
    EXPECT_EQ(profile()->GetPath().AsUTF8Unsafe(), report->id());
    EXPECT_TRUE(report->is_full_report());

    return report;
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
  EXPECT_FALSE(generator_.MaybeGenerate(profile_path, kIdleProfile));
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

}  // namespace enterprise_reporting
