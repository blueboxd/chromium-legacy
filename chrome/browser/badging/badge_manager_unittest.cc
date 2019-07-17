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
#include "chrome/test/base/testing_profile.h"
#include "content/public/test/test_browser_thread_bundle.h"
#include "extensions/common/extension.h"
#include "extensions/common/extension_builder.h"
#include "extensions/common/extension_id.h"
#include "testing/gtest/include/gtest/gtest.h"

using badging::BadgeManager;
using badging::BadgeManagerDelegate;
using badging::BadgeManagerFactory;

namespace {

typedef std::pair<std::string, base::Optional<int>> SetBadgeAction;

const int kBadgeContents = 1;

const extensions::ExtensionId kExtensionId("1");

}  // namespace

namespace badging {

// Testing delegate that records badge changes.
class TestBadgeManagerDelegate : public BadgeManagerDelegate {
 public:
  TestBadgeManagerDelegate() : BadgeManagerDelegate(nullptr) {}

  ~TestBadgeManagerDelegate() override = default;

  void OnBadgeSet(const std::string& app_id,
                  base::Optional<uint64_t> contents) override {
    set_badges_.push_back(std::make_pair(app_id, contents));
  }

  void OnBadgeCleared(const std::string& app_id) override {
    cleared_badges_.push_back(app_id);
  }

  std::vector<std::string>& cleared_badges() { return cleared_badges_; }
  std::vector<SetBadgeAction>& set_badges() { return set_badges_; }

 private:
  std::vector<std::string> cleared_badges_;
  std::vector<SetBadgeAction> set_badges_;
};

class BadgeManagerUnittest : public ::testing::Test {
 public:
  BadgeManagerUnittest() = default;
  ~BadgeManagerUnittest() override = default;

  void SetUp() override {
    profile_.reset(new TestingProfile());

    // Delegate lifetime is managed by BadgeManager
    auto owned_delegate = std::make_unique<TestBadgeManagerDelegate>();
    delegate_ = owned_delegate.get();
    badge_manager_ =
        BadgeManagerFactory::GetInstance()->GetForProfile(profile_.get());
    badge_manager_->SetDelegate(std::move(owned_delegate));
  }

  void TearDown() override { profile_.reset(); }

  TestBadgeManagerDelegate* delegate() { return delegate_; }

  BadgeManager* badge_manager() const { return badge_manager_; }

 private:
  TestBadgeManagerDelegate* delegate_;
  BadgeManager* badge_manager_;
  content::TestBrowserThreadBundle thread_bundle_;
  std::unique_ptr<TestingProfile> profile_;

  DISALLOW_COPY_AND_ASSIGN(BadgeManagerUnittest);
};

TEST_F(BadgeManagerUnittest, SetFlagBadgeForApp) {
  badge_manager()->UpdateAppBadge(kExtensionId, base::nullopt);

  EXPECT_EQ(1UL, delegate()->set_badges().size());
  EXPECT_EQ(kExtensionId, delegate()->set_badges().front().first);
  EXPECT_EQ(base::nullopt, delegate()->set_badges().front().second);
}

TEST_F(BadgeManagerUnittest, SetBadgeForApp) {
  badge_manager()->UpdateAppBadge(kExtensionId, kBadgeContents);

  EXPECT_EQ(1UL, delegate()->set_badges().size());
  EXPECT_EQ(kExtensionId, delegate()->set_badges().front().first);
  EXPECT_EQ(kBadgeContents, delegate()->set_badges().front().second);
}

TEST_F(BadgeManagerUnittest, SetBadgeForMultipleApps) {
  const extensions::ExtensionId otherId("other");
  int otherContents = 2;

  badge_manager()->UpdateAppBadge(kExtensionId, kBadgeContents);
  badge_manager()->UpdateAppBadge(otherId, otherContents);

  EXPECT_EQ(2UL, delegate()->set_badges().size());

  EXPECT_EQ(kExtensionId, delegate()->set_badges()[0].first);
  EXPECT_EQ(kBadgeContents, delegate()->set_badges()[0].second);

  EXPECT_EQ(otherId, delegate()->set_badges()[1].first);
  EXPECT_EQ(otherContents, delegate()->set_badges()[1].second);
}

TEST_F(BadgeManagerUnittest, SetBadgeForAppAfterClear) {
  badge_manager()->UpdateAppBadge(kExtensionId, kBadgeContents);
  badge_manager()->ClearAppBadge(kExtensionId);
  badge_manager()->UpdateAppBadge(kExtensionId, kBadgeContents);

  EXPECT_EQ(2UL, delegate()->set_badges().size());

  EXPECT_EQ(kExtensionId, delegate()->set_badges()[0].first);
  EXPECT_EQ(kBadgeContents, delegate()->set_badges()[0].second);

  EXPECT_EQ(kExtensionId, delegate()->set_badges()[1].first);
  EXPECT_EQ(kBadgeContents, delegate()->set_badges()[1].second);
}

TEST_F(BadgeManagerUnittest, ClearBadgeForBadgedApp) {
  badge_manager()->UpdateAppBadge(kExtensionId, kBadgeContents);

  badge_manager()->ClearAppBadge(kExtensionId);

  EXPECT_EQ(1UL, delegate()->cleared_badges().size());
  EXPECT_EQ(kExtensionId, delegate()->cleared_badges().front());
}

TEST_F(BadgeManagerUnittest, BadgingMultipleProfiles) {
  std::unique_ptr<Profile> other_profile = std::make_unique<TestingProfile>();
  auto* other_badge_manager =
      BadgeManagerFactory::GetInstance()->GetForProfile(other_profile.get());

  auto owned_other_delegate = std::make_unique<TestBadgeManagerDelegate>();
  auto* other_delegate = owned_other_delegate.get();
  other_badge_manager->SetDelegate(std::move(owned_other_delegate));

  other_badge_manager->UpdateAppBadge(kExtensionId, base::nullopt);
  other_badge_manager->UpdateAppBadge(kExtensionId, kBadgeContents);
  other_badge_manager->UpdateAppBadge(kExtensionId, base::nullopt);
  other_badge_manager->ClearAppBadge(kExtensionId);

  badge_manager()->ClearAppBadge(kExtensionId);

  EXPECT_EQ(3UL, other_delegate->set_badges().size());
  EXPECT_EQ(0UL, delegate()->set_badges().size());

  EXPECT_EQ(1UL, other_delegate->cleared_badges().size());
  EXPECT_EQ(1UL, delegate()->cleared_badges().size());

  EXPECT_EQ(kExtensionId, other_delegate->set_badges().back().first);
  EXPECT_EQ(base::nullopt, other_delegate->set_badges().back().second);
}

// Tests methods which call into the badge manager delegate do not crash when
// the delegate is unset.
TEST_F(BadgeManagerUnittest, BadgingWithNoDelegateDoesNotCrash) {
  const std::string kAppId = "app-id";

  badge_manager()->SetDelegate(nullptr);

  badge_manager()->UpdateAppBadge(kAppId, base::nullopt);
  badge_manager()->UpdateAppBadge(kAppId, base::Optional<uint64_t>(7u));
  badge_manager()->UpdateAppBadge(base::nullopt, base::nullopt);
  badge_manager()->UpdateAppBadge(base::nullopt, base::Optional<uint64_t>(7u));

  badge_manager()->ClearAppBadge(kAppId);
  badge_manager()->ClearAppBadge(base::nullopt);
}

}  // namespace badging
