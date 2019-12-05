// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/child_accounts/child_user_service.h"

#include <memory>
#include <vector>

#include "base/feature_list.h"
#include "base/test/scoped_feature_list.h"
#include "base/time/time.h"
#include "chrome/browser/chromeos/child_accounts/child_user_service_factory.h"
#include "chrome/browser/chromeos/child_accounts/time_limits/web_time_limit_interface.h"
#include "chrome/common/chrome_features.h"
#include "chrome/test/base/testing_profile.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace chromeos {

class ChildUserServiceTest : public testing::Test {
 protected:
  ChildUserServiceTest()
      : service_(std::make_unique<ChildUserService>(&profile_)),
        service_test_api_(
            std::make_unique<ChildUserService::TestApi>(service_.get())) {}
  ChildUserServiceTest(const ChildUserServiceTest&) = delete;
  ChildUserServiceTest& operator=(const ChildUserServiceTest&) = delete;
  ~ChildUserServiceTest() override = default;

  // Enables web time limits feature. Recreates ChildUserService object.
  void EnableWebTimeLimits() {
    EnableFeatures({features::kPerAppTimeLimits, features::kWebTimeLimits});
    service_ = std::make_unique<ChildUserService>(&profile_);
    service_test_api_ =
        std::make_unique<ChildUserService::TestApi>(service_.get());
  }

  // Enables per-app time limits feature. Recreates ChildUserService object.
  void EnablePerAppTimeLimits() {
    EnableFeatures({features::kPerAppTimeLimits});
    service_ = std::make_unique<ChildUserService>(&profile_);
    service_test_api_ =
        std::make_unique<ChildUserService::TestApi>(service_.get());
  }

  Profile* profile() { return &profile_; }

  ChildUserService* service() { return service_.get(); }

  ChildUserService::TestApi* service_test_api() {
    return service_test_api_.get();
  }

 private:
  // Enables given |features|. Recreates ChildUserService object.
  void EnableFeatures(const std::vector<base::Feature>& features) {
    scoped_feature_list_.InitWithFeatures(features, {});
    service_ = std::make_unique<ChildUserService>(&profile_);
    service_test_api_ =
        std::make_unique<ChildUserService::TestApi>(service_.get());
  }

  content::BrowserTaskEnvironment task_environment_;

  TestingProfile profile_;

  base::test::ScopedFeatureList scoped_feature_list_;

  std::unique_ptr<ChildUserService> service_;
  std::unique_ptr<ChildUserService::TestApi> service_test_api_;
};

// Tests Per-App Time Limits feature.
using PerAppTimeLimitsTest = ChildUserServiceTest;

TEST_F(PerAppTimeLimitsTest, EnablePerAppTimeLimitsFeature) {
  EXPECT_FALSE(service_test_api()->app_time_controller());
  EXPECT_FALSE(service_test_api()->web_time_enforcer());

  EnablePerAppTimeLimits();

  EXPECT_TRUE(service_test_api()->app_time_controller());
  EXPECT_FALSE(service_test_api()->web_time_enforcer());
}

TEST_F(PerAppTimeLimitsTest, EnableWebTimeLimitsFeature) {
  EXPECT_FALSE(service_test_api()->app_time_controller());
  EXPECT_FALSE(service_test_api()->web_time_enforcer());

  EnableWebTimeLimits();

  EXPECT_TRUE(service_test_api()->app_time_controller());
  EXPECT_TRUE(service_test_api()->web_time_enforcer());
}

TEST_F(PerAppTimeLimitsTest, GetWebTimeLimitInterface) {
  EXPECT_EQ(ChildUserServiceFactory::GetForBrowserContext(profile()),
            app_time::WebTimeLimitInterface::Get(profile()));
}

TEST_F(PerAppTimeLimitsTest, PauseAndResumeWebActivity) {
  EnableWebTimeLimits();
  EXPECT_FALSE(service()->WebTimeLimitReached());

  service()->PauseWebActivity();
  EXPECT_TRUE(service()->WebTimeLimitReached());

  service()->ResumeWebActivity();
  EXPECT_FALSE(service()->WebTimeLimitReached());
  EXPECT_EQ(base::TimeDelta(), service()->GetWebTimeLimit());
}

TEST_F(PerAppTimeLimitsTest, PauseWebActivityTwice) {
  EnableWebTimeLimits();
  EXPECT_FALSE(service()->WebTimeLimitReached());

  service()->PauseWebActivity();
  EXPECT_TRUE(service()->WebTimeLimitReached());

  service()->PauseWebActivity();
  EXPECT_TRUE(service()->WebTimeLimitReached());
}

TEST_F(PerAppTimeLimitsTest, ResumeWebActivityTwice) {
  EnableWebTimeLimits();
  EXPECT_FALSE(service()->WebTimeLimitReached());

  service()->ResumeWebActivity();

  EXPECT_FALSE(service()->WebTimeLimitReached());
  EXPECT_EQ(base::TimeDelta(), service()->GetWebTimeLimit());

  service()->ResumeWebActivity();

  EXPECT_FALSE(service()->WebTimeLimitReached());
  EXPECT_EQ(base::TimeDelta(), service()->GetWebTimeLimit());
}

}  // namespace chromeos
