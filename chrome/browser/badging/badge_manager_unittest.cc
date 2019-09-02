// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/badging/badge_manager.h"

#include <memory>
#include <utility>
#include <vector>

#include "base/optional.h"
#include "chrome/browser/badging/badge_manager_delegate.h"
#include "chrome/browser/badging/badge_manager_factory.h"
#include "chrome/browser/badging/test_badge_manager_delegate.h"
#include "chrome/browser/web_applications/components/web_app_provider_base.h"
#include "chrome/browser/web_applications/test/test_app_registrar.h"
#include "chrome/test/base/testing_profile.h"
#include "content/public/test/browser_task_environment.h"
#include "extensions/common/extension.h"
#include "extensions/common/extension_builder.h"
#include "extensions/common/extension_id.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

using badging::BadgeManager;
using badging::BadgeManagerDelegate;
using badging::BadgeManagerFactory;

namespace {

typedef std::pair<GURL, base::Optional<int>> SetBadgeAction;

constexpr uint64_t kBadgeContents = 1;
const GURL kAppScope("https://example.com/app");

}  // namespace

namespace badging {

class BadgeManagerUnittest : public ::testing::Test {
 public:
  BadgeManagerUnittest() = default;
  ~BadgeManagerUnittest() override = default;

  void SetUp() override {
    profile_.reset(new TestingProfile());
    registrar_.reset(new web_app::TestAppRegistrar());

    badge_manager_ =
        BadgeManagerFactory::GetInstance()->GetForProfile(profile_.get());

    // Delegate lifetime is managed by BadgeManager
    auto owned_delegate = std::make_unique<TestBadgeManagerDelegate>(
        profile_.get(), badge_manager_, registrar_.get());
    delegate_ = owned_delegate.get();
    badge_manager_->SetDelegate(std::move(owned_delegate));
  }

  void TearDown() override { profile_.reset(); }

  TestBadgeManagerDelegate* delegate() { return delegate_; }

  BadgeManager* badge_manager() const { return badge_manager_; }

 private:
  std::unique_ptr<web_app::TestAppRegistrar> registrar_;
  TestBadgeManagerDelegate* delegate_;
  BadgeManager* badge_manager_;
  content::BrowserTaskEnvironment task_environment_;
  std::unique_ptr<TestingProfile> profile_;

  DISALLOW_COPY_AND_ASSIGN(BadgeManagerUnittest);
};

TEST_F(BadgeManagerUnittest, SetFlagBadgeForApp) {
  badge_manager()->SetBadgeForTesting(kAppScope, base::nullopt);

  EXPECT_EQ(1UL, delegate()->set_scope_badges().size());
  EXPECT_EQ(kAppScope, delegate()->set_scope_badges().front().first);
  EXPECT_EQ(base::nullopt, delegate()->set_scope_badges().front().second);
}

TEST_F(BadgeManagerUnittest, SetBadgeForApp) {
  badge_manager()->SetBadgeForTesting(kAppScope,
                                      base::make_optional(kBadgeContents));

  EXPECT_EQ(1UL, delegate()->set_scope_badges().size());
  EXPECT_EQ(kAppScope, delegate()->set_scope_badges().front().first);
  EXPECT_EQ(kBadgeContents, delegate()->set_scope_badges().front().second);
}

TEST_F(BadgeManagerUnittest, SetBadgeForMultipleApps) {
  const GURL kOtherScope("http://other.app/other");
  constexpr uint64_t kOtherContents = 2;

  badge_manager()->SetBadgeForTesting(kAppScope,
                                      base::make_optional(kBadgeContents));
  badge_manager()->SetBadgeForTesting(kOtherScope,
                                      base::make_optional(kOtherContents));

  EXPECT_EQ(2UL, delegate()->set_scope_badges().size());

  EXPECT_EQ(kAppScope, delegate()->set_scope_badges()[0].first);
  EXPECT_EQ(kBadgeContents, delegate()->set_scope_badges()[0].second);

  EXPECT_EQ(kOtherScope, delegate()->set_scope_badges()[1].first);
  EXPECT_EQ(kOtherContents, delegate()->set_scope_badges()[1].second);
}

TEST_F(BadgeManagerUnittest, SetBadgeForAppAfterClear) {
  badge_manager()->SetBadgeForTesting(kAppScope,
                                      base::make_optional(kBadgeContents));
  badge_manager()->ClearBadgeForTesting(kAppScope);
  badge_manager()->SetBadgeForTesting(kAppScope,
                                      base::make_optional(kBadgeContents));

  EXPECT_EQ(2UL, delegate()->set_scope_badges().size());

  EXPECT_EQ(kAppScope, delegate()->set_scope_badges()[0].first);
  EXPECT_EQ(kBadgeContents, delegate()->set_scope_badges()[0].second);

  EXPECT_EQ(kAppScope, delegate()->set_scope_badges()[1].first);
  EXPECT_EQ(kBadgeContents, delegate()->set_scope_badges()[1].second);
}

TEST_F(BadgeManagerUnittest, ClearBadgeForBadgedApp) {
  badge_manager()->SetBadgeForTesting(kAppScope,
                                      base::make_optional(kBadgeContents));
  badge_manager()->ClearBadgeForTesting(kAppScope);

  EXPECT_EQ(1UL, delegate()->cleared_scope_badges().size());
  EXPECT_EQ(kAppScope, delegate()->cleared_scope_badges().front());
}

TEST_F(BadgeManagerUnittest, BadgingMultipleProfiles) {
  std::unique_ptr<Profile> other_profile = std::make_unique<TestingProfile>();
  auto* other_badge_manager =
      BadgeManagerFactory::GetInstance()->GetForProfile(other_profile.get());

  web_app::TestAppRegistrar other_registrar;
  auto owned_other_delegate = std::make_unique<TestBadgeManagerDelegate>(
      other_profile.get(), other_badge_manager, &other_registrar);
  auto* other_delegate = owned_other_delegate.get();
  other_badge_manager->SetDelegate(std::move(owned_other_delegate));

  other_badge_manager->SetBadgeForTesting(kAppScope, base::nullopt);
  other_badge_manager->SetBadgeForTesting(kAppScope,
                                          base::make_optional(kBadgeContents));
  other_badge_manager->SetBadgeForTesting(kAppScope, base::nullopt);
  other_badge_manager->ClearBadgeForTesting(kAppScope);

  badge_manager()->ClearBadgeForTesting(kAppScope);

  EXPECT_EQ(3UL, other_delegate->set_scope_badges().size());
  EXPECT_EQ(0UL, delegate()->set_scope_badges().size());

  EXPECT_EQ(1UL, other_delegate->cleared_scope_badges().size());
  EXPECT_EQ(1UL, delegate()->cleared_scope_badges().size());

  EXPECT_EQ(kAppScope, other_delegate->set_scope_badges().back().first);
  EXPECT_EQ(base::nullopt, other_delegate->set_scope_badges().back().second);
}

// Tests methods which call into the badge manager delegate do not crash when
// the delegate is unset.
TEST_F(BadgeManagerUnittest, BadgingWithNoDelegateDoesNotCrash) {
  badge_manager()->SetDelegate(nullptr);

  badge_manager()->SetBadgeForTesting(kAppScope, base::nullopt);
  badge_manager()->SetBadgeForTesting(kAppScope,
                                      base::make_optional(kBadgeContents));
  badge_manager()->SetBadgeForTesting(GURL(), base::nullopt);
  badge_manager()->SetBadgeForTesting(GURL(),
                                      base::make_optional(kBadgeContents));
  badge_manager()->ClearBadgeForTesting(GURL());
}

TEST_F(BadgeManagerUnittest, MostSpecificBadgeApplies) {
  const GURL origin("https://example.com/");
  const GURL app("https://example.com/app");
  const GURL app_page("https://example.com/app/page/1");

  const base::Optional<BadgeManager::BadgeValue> origin_badge =
      base::make_optional(base::Optional<uint64_t>(1U));
  const base::Optional<BadgeManager::BadgeValue> app_badge =
      base::make_optional(base::Optional<uint64_t>(1U));
  const base::Optional<BadgeManager::BadgeValue> app_page_badge =
      base::make_optional(base::Optional<uint64_t>(1U));

  badge_manager()->SetBadgeForTesting(origin, origin_badge.value());
  EXPECT_EQ(badge_manager()->GetBadgeValue(origin), origin_badge);
  EXPECT_EQ(badge_manager()->GetBadgeValue(app), origin_badge);
  EXPECT_EQ(badge_manager()->GetBadgeValue(app_page), origin_badge);

  badge_manager()->SetBadgeForTesting(app, app_badge.value());
  EXPECT_EQ(badge_manager()->GetBadgeValue(origin), origin_badge);
  EXPECT_EQ(badge_manager()->GetBadgeValue(app), app_badge);
  EXPECT_EQ(badge_manager()->GetBadgeValue(app_page), app_badge);

  badge_manager()->SetBadgeForTesting(app_page, app_page_badge.value());
  EXPECT_EQ(badge_manager()->GetBadgeValue(origin), origin_badge);
  EXPECT_EQ(badge_manager()->GetBadgeValue(app), app_badge);
  EXPECT_EQ(badge_manager()->GetBadgeValue(app_page), app_page_badge);

  badge_manager()->ClearBadgeForTesting(app);
  EXPECT_EQ(badge_manager()->GetBadgeValue(origin), origin_badge);
  EXPECT_EQ(badge_manager()->GetBadgeValue(app), origin_badge);
  EXPECT_EQ(badge_manager()->GetBadgeValue(app_page), app_page_badge);
}

}  // namespace badging
