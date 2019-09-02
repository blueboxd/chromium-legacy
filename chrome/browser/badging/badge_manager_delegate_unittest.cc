// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>
#include <utility>

#include "base/optional.h"
#include "chrome/browser/badging/badge_manager.h"
#include "chrome/browser/badging/badge_manager_factory.h"
#include "chrome/browser/badging/test_badge_manager_delegate.h"
#include "chrome/browser/web_applications/components/web_app_helpers.h"
#include "chrome/browser/web_applications/test/test_app_registrar.h"
#include "chrome/test/base/testing_profile.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace badging {

class BadgeManagerDelegateUnittest : public ::testing::Test {
 public:
  BadgeManagerDelegateUnittest() = default;
  ~BadgeManagerDelegateUnittest() override = default;

  void SetUp() override {
    profile_.reset(new TestingProfile());
    registrar_.reset(new web_app::TestAppRegistrar());

    badge_manager_ =
        BadgeManagerFactory::GetInstance()->GetForProfile(profile_.get());

    // Delegate lifetime is managed by BadgeManager.
    auto owned_delegate = std::make_unique<TestBadgeManagerDelegate>(
        profile_.get(), badge_manager_, registrar());
    delegate_ = owned_delegate.get();
    badge_manager_->SetDelegate(std::move(owned_delegate));
  }

  void TearDown() override { profile_.reset(); }

  TestBadgeManagerDelegate* delegate() { return delegate_; }

  web_app::TestAppRegistrar* registrar() const { return registrar_.get(); }
  BadgeManager* badge_manager() const { return badge_manager_; }

 private:
  std::unique_ptr<web_app::TestAppRegistrar> registrar_;
  TestBadgeManagerDelegate* delegate_;
  BadgeManager* badge_manager_;
  std::unique_ptr<TestingProfile> profile_;
  content::BrowserTaskEnvironment task_environment_;

  DISALLOW_COPY_AND_ASSIGN(BadgeManagerDelegateUnittest);
};

TEST_F(BadgeManagerDelegateUnittest, InScopeAppsAreBadged) {
  const web_app::AppId& kApp1 = "app1";
  const web_app::AppId& kApp2 = "app2";

  registrar()->AddExternalApp(kApp1, {GURL("https://example.com/app1")});
  registrar()->AddExternalApp(kApp2, {GURL("https://example.com/app2")});

  badge_manager()->SetBadgeForTesting(GURL("https://example.com/"),
                                      base::nullopt);

  EXPECT_EQ(2UL, delegate()->set_app_badges().size());

  EXPECT_EQ(kApp1, delegate()->set_app_badges()[0].first);
  EXPECT_EQ(base::nullopt, delegate()->set_app_badges()[0].second);

  EXPECT_EQ(kApp2, delegate()->set_app_badges()[1].first);
  EXPECT_EQ(base::nullopt, delegate()->set_app_badges()[1].second);
}

TEST_F(BadgeManagerDelegateUnittest, InScopeAppsAreCleared) {
  const web_app::AppId& kApp1 = "app1";
  const web_app::AppId& kApp2 = "app2";

  registrar()->AddExternalApp(kApp1, {GURL("https://example.com/app1")});
  registrar()->AddExternalApp(kApp2, {GURL("https://example.com/app2")});

  badge_manager()->SetBadgeForTesting(GURL("https://example.com/"),
                                      base::nullopt);
  badge_manager()->ClearBadgeForTesting(GURL("https://example.com/"));

  EXPECT_EQ(2UL, delegate()->cleared_app_badges().size());

  EXPECT_EQ(kApp1, delegate()->cleared_app_badges()[0]);
  EXPECT_EQ(kApp2, delegate()->cleared_app_badges()[1]);
}

}  // namespace badging
