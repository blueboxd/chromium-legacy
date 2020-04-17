// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/tab/state/tab_state_db_factory.h"

#include "chrome/test/base/testing_profile.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gtest/include/gtest/gtest.h"

class TabStateDBFactoryTest : public testing::Test {
 public:
  TabStateDBFactoryTest() = default;

  Profile* profile() { return &profile_; }
  Profile* different_profile() { return &different_profile_; }

 private:
  content::BrowserTaskEnvironment task_environment_;
  TestingProfile profile_;
  TestingProfile different_profile_;

  DISALLOW_COPY_AND_ASSIGN(TabStateDBFactoryTest);
};

TEST_F(TabStateDBFactoryTest, TestIncognitoProfile) {
  EXPECT_EQ(nullptr, TabStateDBFactory::GetInstance()->GetForProfile(
                         profile()->GetOffTheRecordProfile()));
}

TEST_F(TabStateDBFactoryTest, TestSameProfile) {
  EXPECT_EQ(TabStateDBFactory::GetInstance()->GetForProfile(profile()),
            TabStateDBFactory::GetInstance()->GetForProfile(profile()));
}

TEST_F(TabStateDBFactoryTest, TestDifferentProfile) {
  EXPECT_NE(
      TabStateDBFactory::GetInstance()->GetForProfile(different_profile()),
      TabStateDBFactory::GetInstance()->GetForProfile(profile()));
}
