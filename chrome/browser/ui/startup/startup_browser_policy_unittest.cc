// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/policy/browser_signin_policy_handler.h"
#include "chrome/browser/profile_resetter/profile_resetter_test_base.h"
#include "chrome/browser/search_engines/template_url_service_factory.h"
#include "chrome/browser/ui/webui/welcome/helpers.h"
#include "chrome/test/base/testing_profile.h"
#include "components/policy/core/common/mock_policy_service.h"
#include "components/policy/core/common/policy_map.h"
#include "components/policy/policy_constants.h"
#include "content/public/test/test_browser_thread_bundle.h"
#include "testing/gtest/include/gtest/gtest.h"

class StartupBrowserPolicyUnitTest : public testing::Test {
 protected:
  StartupBrowserPolicyUnitTest() = default;
  ~StartupBrowserPolicyUnitTest() override = default;

  std::unique_ptr<policy::MockPolicyService> GetPolicyService(
      policy::PolicyMap& policy_map) {
    auto policy_service = std::make_unique<policy::MockPolicyService>();
    ON_CALL(*policy_service.get(), GetPolicies(testing::_))
        .WillByDefault(testing::ReturnRef(policy_map));
    return policy_service;
  }

  template <typename... Args>
  void SetPolicy(policy::PolicyMap& policy_map,
                 const std::string& policy,
                 Args... args) {
    policy_map.Set(policy, policy::POLICY_LEVEL_MANDATORY,
                   policy::POLICY_SCOPE_USER, policy::POLICY_SOURCE_CLOUD,
                   std::make_unique<base::Value>(args...), nullptr);
  }

  template <typename... Args>
  std::unique_ptr<policy::PolicyMap> MakePolicy(const std::string& policy,
                                                Args... args) {
    auto policy_map = std::make_unique<policy::PolicyMap>();
    SetPolicy(*policy_map.get(), policy, args...);
    return policy_map;
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(StartupBrowserPolicyUnitTest);
};

TEST_F(StartupBrowserPolicyUnitTest, BookmarkBarEnabled) {
  EXPECT_TRUE(welcome::CanShowGoogleAppModuleForTesting(policy::PolicyMap()));

  auto policy_map = MakePolicy(policy::key::kBookmarkBarEnabled, true);
  EXPECT_TRUE(welcome::CanShowGoogleAppModuleForTesting(*policy_map));

  policy_map = MakePolicy(policy::key::kBookmarkBarEnabled, false);
  EXPECT_FALSE(welcome::CanShowGoogleAppModuleForTesting(*policy_map));
}

TEST_F(StartupBrowserPolicyUnitTest, EditBookmarksEnabled) {
  EXPECT_TRUE(welcome::CanShowGoogleAppModuleForTesting(policy::PolicyMap()));

  auto policy_map = MakePolicy(policy::key::kEditBookmarksEnabled, true);
  EXPECT_TRUE(welcome::CanShowGoogleAppModuleForTesting(*policy_map));

  policy_map = MakePolicy(policy::key::kEditBookmarksEnabled, false);
  EXPECT_FALSE(welcome::CanShowGoogleAppModuleForTesting(*policy_map));
}

TEST_F(StartupBrowserPolicyUnitTest, DefaultBrowserSettingEnabled) {
  EXPECT_TRUE(welcome::CanShowSetDefaultModuleForTesting(policy::PolicyMap()));

  auto policy_map =
      MakePolicy(policy::key::kDefaultBrowserSettingEnabled, true);
  EXPECT_TRUE(welcome::CanShowSetDefaultModuleForTesting(*policy_map));

  policy_map = MakePolicy(policy::key::kDefaultBrowserSettingEnabled, false);
  EXPECT_FALSE(welcome::CanShowSetDefaultModuleForTesting(*policy_map));
}

TEST_F(StartupBrowserPolicyUnitTest, BrowserSignin) {
  EXPECT_TRUE(welcome::CanShowSigninModuleForTesting(policy::PolicyMap()));

  auto policy_map =
      MakePolicy(policy::key::kBrowserSignin,
                 static_cast<int>(policy::BrowserSigninMode::kEnabled));
  EXPECT_TRUE(welcome::CanShowSigninModuleForTesting(*policy_map));

  policy_map = MakePolicy(policy::key::kBrowserSignin,
                          static_cast<int>(policy::BrowserSigninMode::kForced));
  EXPECT_TRUE(welcome::CanShowSigninModuleForTesting(*policy_map));

  policy_map =
      MakePolicy(policy::key::kBrowserSignin,
                 static_cast<int>(policy::BrowserSigninMode::kDisabled));
  EXPECT_FALSE(welcome::CanShowSigninModuleForTesting(*policy_map));
}

TEST_F(StartupBrowserPolicyUnitTest, ForceEphemeralProfiles) {
  policy::PolicyMap policy_map;
  TestingProfile::Builder builder;
  builder.SetPolicyService(GetPolicyService(policy_map));
  // Needed by the builder when building the profile.
  content::BrowserTaskEnvironment task_environment;
  auto profile = builder.Build();

  EXPECT_TRUE(welcome::HasModulesToShow(profile.get()));

  SetPolicy(policy_map, policy::key::kForceEphemeralProfiles, true);
  EXPECT_FALSE(welcome::HasModulesToShow(profile.get()));

  SetPolicy(policy_map, policy::key::kForceEphemeralProfiles, false);
  EXPECT_TRUE(welcome::HasModulesToShow(profile.get()));
}

TEST_F(StartupBrowserPolicyUnitTest, NewTabPageLocation) {
  policy::PolicyMap policy_map;
  TestingProfile::Builder builder;
  builder.SetPolicyService(GetPolicyService(policy_map));
  // Needed by the builder when building the profile.
  content::BrowserTaskEnvironment task_environment;
  auto profile = builder.Build();

  TemplateURLServiceFactory::GetInstance()->SetTestingFactory(
      profile.get(), base::BindRepeating(&CreateTemplateURLServiceForTesting));

  EXPECT_TRUE(
      welcome::CanShowNTPBackgroundModuleForTesting(policy_map, profile.get()));

  SetPolicy(policy_map, policy::key::kNewTabPageLocation, "https://crbug.com");
  EXPECT_FALSE(
      welcome::CanShowNTPBackgroundModuleForTesting(policy_map, profile.get()));
}
