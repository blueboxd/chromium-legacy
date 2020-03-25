// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/enterprise/reporting/notification/extension_request_observer_factory.h"

#include "chrome/browser/enterprise/reporting/notification/extension_request_observer.h"
#include "chrome/test/base/testing_browser_process.h"
#include "chrome/test/base/testing_profile_manager.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace enterprise_reporting {
constexpr char kProfile1[] = "profile-1";
constexpr char kProfile2[] = "profile-2";

class ExtensionRequestObserverFactoryTest : public ::testing::Test {
 public:
  void SetUp() override { ASSERT_TRUE(profile_manager_.SetUp()); }
  TestingProfileManager* profile_manager() { return &profile_manager_; }

 private:
  content::BrowserTaskEnvironment task_environment_;
  TestingProfileManager profile_manager_{TestingBrowserProcess::GetGlobal()};
};

TEST_F(ExtensionRequestObserverFactoryTest, LoadExistProfile) {
  TestingProfile* profile = profile_manager()->CreateTestingProfile(kProfile1);
  ExtensionRequestObserverFactory factory_;
  EXPECT_TRUE(factory_.GetObserverByProfileForTesting(profile));
  EXPECT_EQ(1, factory_.GetNumberOfObserversForTesting());
}

TEST_F(ExtensionRequestObserverFactoryTest, AddProfile) {
  ExtensionRequestObserverFactory factory_;
  EXPECT_EQ(0, factory_.GetNumberOfObserversForTesting());

  TestingProfile* profile1 = profile_manager()->CreateTestingProfile(kProfile1);
  EXPECT_TRUE(factory_.GetObserverByProfileForTesting(profile1));
  EXPECT_EQ(1, factory_.GetNumberOfObserversForTesting());

  TestingProfile* profile2 = profile_manager()->CreateTestingProfile(kProfile2);
  EXPECT_TRUE(factory_.GetObserverByProfileForTesting(profile2));
  EXPECT_EQ(2, factory_.GetNumberOfObserversForTesting());
}

TEST_F(ExtensionRequestObserverFactoryTest,
       NoObserverForSystemAndGuestProfile) {
  ExtensionRequestObserverFactory factory_;
  EXPECT_EQ(0, factory_.GetNumberOfObserversForTesting());

  TestingProfile* guest_profile = profile_manager()->CreateGuestProfile();
  EXPECT_FALSE(factory_.GetObserverByProfileForTesting(guest_profile));
  EXPECT_EQ(0, factory_.GetNumberOfObserversForTesting());

  TestingProfile* system_profile = profile_manager()->CreateSystemProfile();
  EXPECT_FALSE(factory_.GetObserverByProfileForTesting(system_profile));
  EXPECT_EQ(0, factory_.GetNumberOfObserversForTesting());
}

}  // namespace enterprise_reporting
