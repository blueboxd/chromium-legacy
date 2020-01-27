// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/updates/announcement_notification/announcement_notification_service.h"

#include <memory>

#include "base/files/file_path.h"
#include "base/memory/ptr_util.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/browser/profiles/profile_attributes_storage.h"
#include "chrome/test/base/testing_browser_process.h"
#include "chrome/test/base/testing_profile.h"
#include "chrome/test/base/testing_profile_manager.h"
#include "components/account_id/account_id.h"
#include "components/prefs/testing_pref_service.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using testing::_;
using testing::NiceMock;
using testing::Return;

namespace {

const char kProfileId[] = "dummy_profile";
const char kRemoteUrl[] = "www.example.com";

class MockDelegate : public AnnouncementNotificationService::Delegate {
 public:
  MockDelegate() = default;
  ~MockDelegate() override = default;
  MOCK_METHOD1(ShowNotification, void(const std::string&));
  MOCK_METHOD0(IsFirstRun, bool());

 private:
  DISALLOW_COPY_AND_ASSIGN(MockDelegate);
};

class AnnouncementNotificationServiceTest : public testing::Test {
 public:
  AnnouncementNotificationServiceTest() = default;
  ~AnnouncementNotificationServiceTest() override = default;

 protected:
  AnnouncementNotificationService* service() {
    DCHECK(service_) << "Call Init() first.";
    return service_.get();
  }

  MockDelegate* delegate() { return delegate_; }

  int CurrentVersionPref() {
    return pref_service_->GetInteger(kCurrentVersionPrefName);
  }

  void InitProfile(bool sign_in) {
    test_profile_manager_.reset(
        new TestingProfileManager(TestingBrowserProcess::GetGlobal()));
    ASSERT_TRUE(test_profile_manager_->SetUp());
    test_profile_ =
        test_profile_manager_->CreateTestingProfile("dummy@gmail.com");
    std::string gaia_id = sign_in ? "dummy_gaia_id" : std::string();
    test_profile_manager_->profile_attributes_storage()->AddProfile(
        test_profile_manager_->profiles_dir().AppendASCII(kProfileId),
        base::ASCIIToUTF16("dummy_name"), gaia_id, base::string16(),
        sign_in /*is_consented_primary_account*/, 0, std::string(),
        EmptyAccountId());
  }

  void Init(bool enable_feature,
            bool skip_first_run,
            int version,
            int current_version) {
    // Setup Finch config.
    std::map<std::string, std::string> parameters = {
        {kSkipFirstRun, skip_first_run ? "true" : "false"},
        {kVersion, base::NumberToString(version)}};
    InitProfile(false);
    Init(parameters, enable_feature, current_version, false);
  }

  void Init(const std::map<std::string, std::string>& parameters,
            bool enable_feature,
            int current_version,
            bool new_profile) {
    if (enable_feature) {
      scoped_feature_list_.InitAndEnableFeatureWithParameters(
          kAnnouncementNotification, parameters);
    } else {
      scoped_feature_list_.InitAndDisableFeature(kAnnouncementNotification);
    }

    // Register pref.
    pref_service_ = std::make_unique<TestingPrefServiceSimple>();
    AnnouncementNotificationService::RegisterProfilePrefs(
        pref_service_->registry());
    pref_service_->SetInteger(kCurrentVersionPrefName, current_version);

    // Setup test target objects.
    auto delegate = std::make_unique<NiceMock<MockDelegate>>();
    delegate_ = delegate.get();
    service_ = base::WrapUnique<AnnouncementNotificationService>(
        AnnouncementNotificationService::Create(
            test_profile_manager_->profiles_dir().AppendASCII(kProfileId),
            new_profile, pref_service_.get(), std::move(delegate)));
  }

 private:
  content::BrowserTaskEnvironment task_environment_;
  TestingProfile* test_profile_;
  std::unique_ptr<TestingProfileManager> test_profile_manager_;
  base::test::ScopedFeatureList scoped_feature_list_;
  std::unique_ptr<AnnouncementNotificationService> service_;
  std::unique_ptr<TestingPrefServiceSimple> pref_service_;
  MockDelegate* delegate_ = nullptr;
  DISALLOW_COPY_AND_ASSIGN(AnnouncementNotificationServiceTest);
};

TEST_F(AnnouncementNotificationServiceTest, RequireSignOut) {
  std::map<std::string, std::string> parameters = {
      {kSkipFirstRun, "false"}, {kVersion, "2"}, {kRequireSignout, "true"}};
  // Profile now is signed in.
  InitProfile(true /*sign_in*/);
  Init(parameters, true, 1, false);

  ON_CALL(*delegate(), IsFirstRun()).WillByDefault(Return(false));
  EXPECT_CALL(*delegate(), ShowNotification(_)).Times(0);
  service()->MaybeShowNotification();
  EXPECT_EQ(CurrentVersionPref(), 2);
}

TEST_F(AnnouncementNotificationServiceTest, SkipNewProfile) {
  std::map<std::string, std::string> parameters = {
      {kSkipFirstRun, "false"}, {kVersion, "2"}, {kSkipNewProfile, "true"}};
  // Profile now is signed in.
  InitProfile(false);
  Init(parameters, true, 1, true /*new_profile*/);

  ON_CALL(*delegate(), IsFirstRun()).WillByDefault(Return(false));
  EXPECT_CALL(*delegate(), ShowNotification(_)).Times(0);
  service()->MaybeShowNotification();
  EXPECT_EQ(CurrentVersionPref(), 2);
}

TEST_F(AnnouncementNotificationServiceTest, RemoteUrl) {
  std::map<std::string, std::string> parameters = {
      {kSkipFirstRun, "false"},
      {kVersion, "4"},
      {kSkipNewProfile, "true"},
      {kAnnouncementUrl, kRemoteUrl}};
  // Profile now is signed in.
  InitProfile(false);
  Init(parameters, true, 1, false);
  ON_CALL(*delegate(), IsFirstRun()).WillByDefault(Return(false));
  EXPECT_CALL(*delegate(), ShowNotification(kRemoteUrl));
  service()->MaybeShowNotification();
  EXPECT_EQ(CurrentVersionPref(), 4);
}

struct VersionTestParam {
  bool enable_feature;
  bool skip_first_run;
  bool is_first_run;
  int version;
  int current_version;
  bool show_notification_called;
  int expected_version_pref;
};

class AnnouncementNotificationServiceVersionTest
    : public AnnouncementNotificationServiceTest,
      public ::testing::WithParamInterface<VersionTestParam> {
 public:
  AnnouncementNotificationServiceVersionTest() = default;
  ~AnnouncementNotificationServiceVersionTest() override = default;

 private:
  DISALLOW_COPY_AND_ASSIGN(AnnouncementNotificationServiceVersionTest);
};

const VersionTestParam kVersionTestParams[] = {
    // First run. No current version in pref, has Finch parameter.
    {true, false /*skip_first_run*/, true /*is_first_run*/, 1, -1, true, 1},
    // Skip first run.
    {true, true /*skip_first_run*/, true /*is_first_run*/, 2, -1, false, 2},
    {true, true /*skip_first_run*/, false /*is_first_run*/, 2, -1, true, 2},
    // DisableFeature
    {false /*enable_feature*/, false, false, 1, -1, false, -1},
    // Same version between Finch parameter and preference.
    {true, false, false, 3 /*version*/, 3 /*current_version*/, false, 3},
    // New version from Finch parameter.
    {true, false, false, 4 /*version*/, 3 /*current_version*/, true, 4},
    // OldVersion
    {true, false, false, 2 /*version*/, 3 /*current_version*/, false, 2},
    // No current version in pref, no Finch parameter.
    {true, false, false, -1 /*version*/, -1 /*current_version*/, false, -1},
    // Has current version in pref, no Finch parameter.
    {true, false, false, -1 /*version*/, 10 /*current_version*/, false, 10},
};

TEST_P(AnnouncementNotificationServiceVersionTest, VersionTest) {
  const auto& param = GetParam();
  Init(param.enable_feature, param.skip_first_run, param.version,
       param.current_version);
  ON_CALL(*delegate(), IsFirstRun()).WillByDefault(Return(param.is_first_run));
  EXPECT_CALL(*delegate(), ShowNotification(std::string()))
      .Times(param.show_notification_called ? 1 : 0);
  service()->MaybeShowNotification();
  EXPECT_EQ(CurrentVersionPref(), param.expected_version_pref);
}

INSTANTIATE_TEST_SUITE_P(All,
                         AnnouncementNotificationServiceVersionTest,
                         testing::ValuesIn(kVersionTestParams));

}  // namespace
