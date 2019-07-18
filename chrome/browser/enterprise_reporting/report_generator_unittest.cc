// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/enterprise_reporting/report_generator.h"

#include <set>

#include "base/logging.h"
#include "base/strings/string16.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/utf_string_conversions.h"
#include "build/build_config.h"
#include "chrome/browser/profiles/profile_attributes_storage.h"
#include "chrome/test/base/testing_browser_process.h"
#include "chrome/test/base/testing_profile_manager.h"
#include "components/account_id/account_id.h"
#include "content/public/test/test_browser_thread_bundle.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace em = enterprise_management;

namespace enterprise_reporting {
namespace {
#if !defined(OS_CHROMEOS)
// We only upload serial number on Windows.
void VerifySerialNumber(const std::string& serial_number) {
#if defined(OS_WIN)
  EXPECT_NE(std::string(), serial_number);
#else
  EXPECT_EQ(std::string(), serial_number);
#endif
}

const char kProfile1[] = "Profile";
#endif
}  // namespace

#if !defined(OS_CHROMEOS)
class ReportGeneratorTest : public ::testing::Test {
 public:
  ReportGeneratorTest()
      : profile_manager_(TestingBrowserProcess::GetGlobal()) {}
  ~ReportGeneratorTest() override = default;

  void SetUp() override { ASSERT_TRUE(profile_manager_.SetUp()); }

  // Creates |number| of Profiles. Returns the set of their names. The profile
  // names begin with Profile|start_index|. Profile instances are created if
  // |is_active| is true. Otherwise, information is only put into
  // ProfileAttributesStorage.
  std::set<std::string> CreateProfiles(int number,
                                       bool is_active,
                                       int start_index = 0) {
    std::set<std::string> profile_names;
    for (int i = start_index; i < number; i++) {
      std::string profile_name =
          std::string(kProfile1) + base::NumberToString(i);
      if (is_active) {
        profile_manager_.CreateTestingProfile(profile_name);
      } else {
        profile_manager_.profile_attributes_storage()->AddProfile(
            profile_manager()->profiles_dir().AppendASCII(profile_name),
            base::ASCIIToUTF16(profile_name), std::string(), base::string16(),
            0, std::string(), EmptyAccountId());
      }
      profile_names.insert(profile_name);
    }
    profile_manager_.CreateGuestProfile();
    profile_manager_.CreateSystemProfile();
    return profile_names;
  }

  TestingProfileManager* profile_manager() { return &profile_manager_; }

 private:
  content::TestBrowserThreadBundle thread_bundle_;
  TestingProfileManager profile_manager_;

  DISALLOW_COPY_AND_ASSIGN(ReportGeneratorTest);
};

TEST_F(ReportGeneratorTest, GenerateBasicReport) {
  int profile_number = 2;
  auto profile_names = CreateProfiles(profile_number, /*is_active=*/false);

  ReportGenerator generator;
  auto requests = generator.Generate();
  EXPECT_EQ(1u, requests.size());

  auto* basic_request = requests[0].get();
  EXPECT_NE(std::string(), basic_request->computer_name());
  EXPECT_NE(std::string(), basic_request->os_user_name());
  VerifySerialNumber(basic_request->serial_number());

  EXPECT_TRUE(basic_request->has_os_report());
  auto& os_report = basic_request->os_report();
  EXPECT_NE(std::string(), os_report.name());
  EXPECT_NE(std::string(), os_report.arch());
  EXPECT_NE(std::string(), os_report.version());

  EXPECT_TRUE(basic_request->has_browser_report());
  auto& browser_report = basic_request->browser_report();
  EXPECT_NE(std::string(), browser_report.browser_version());
  EXPECT_NE(std::string(), browser_report.executable_path());
  EXPECT_TRUE(browser_report.has_channel());

  EXPECT_EQ(profile_number, browser_report.profile_info_list_size());
  for (int i = 0; i < profile_number; i++) {
    auto profile_info = browser_report.profile_info_list(i);
    std::string profile_name = profile_info.name();

    // Verify that the profile id is set as profile path.
    EXPECT_EQ(profile_manager()
                  ->profiles_dir()
                  .AppendASCII(profile_name)
                  .AsUTF8Unsafe(),
              profile_info.id());

    // Verify that the basic report does not contain any full Profile report.
    EXPECT_FALSE(profile_info.is_full_report());

    // Verify the profile name is one of the profiles that were created above.
    auto it = profile_names.find(profile_name);
    EXPECT_NE(profile_names.end(), it);
    profile_names.erase(it);
  }
}
#endif

}  // namespace enterprise_reporting
