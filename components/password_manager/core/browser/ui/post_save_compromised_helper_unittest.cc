// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/password_manager/core/browser/ui/post_save_compromised_helper.h"

#include "base/strings/utf_string_conversions.h"
#include "base/test/mock_callback.h"
#include "base/test/task_environment.h"
#include "components/password_manager/core/browser/mock_password_store.h"
#include "components/password_manager/core/common/password_manager_pref_names.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/testing_pref_service.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace password_manager {
namespace {

using BubbleType = PostSaveCompromisedHelper::BubbleType;
using prefs::kLastTimePasswordCheckCompleted;
using testing::Return;

constexpr char kSignonRealm[] = "https://example.com/";
constexpr char kUsername[] = "user";
constexpr char kUsername2[] = "user2";

CompromisedCredentials CreateCompromised(base::StringPiece username) {
  return CompromisedCredentials{
      .signon_realm = kSignonRealm,
      .username = base::ASCIIToUTF16(username),
      .compromise_type = CompromiseType::kLeaked,
  };
}

}  // namespace

class PostSaveCompromisedHelperTest : public testing::Test {
 public:
  PostSaveCompromisedHelperTest() {
    mock_store_ = new MockPasswordStore;
    EXPECT_TRUE(mock_store_->Init(&prefs_));
    prefs_.registry()->RegisterDoublePref(kLastTimePasswordCheckCompleted, 0.0);
  }

  ~PostSaveCompromisedHelperTest() override {
    mock_store_->ShutdownOnUIThread();
  }

  void WaitForPasswordStore() { task_environment_.RunUntilIdle(); }

  MockPasswordStore* store() { return mock_store_.get(); }
  TestingPrefServiceSimple* prefs() { return &prefs_; }

 private:
  base::test::SingleThreadTaskEnvironment task_environment_{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};
  scoped_refptr<MockPasswordStore> mock_store_;
  TestingPrefServiceSimple prefs_;
};

TEST_F(PostSaveCompromisedHelperTest, DefaultState) {
  PostSaveCompromisedHelper helper({}, base::ASCIIToUTF16(kUsername));
  EXPECT_EQ(BubbleType::kNoBubble, helper.bubble_type());
  EXPECT_EQ(0u, helper.compromised_count());
}

TEST_F(PostSaveCompromisedHelperTest, EmptyStore) {
  PostSaveCompromisedHelper helper({}, base::ASCIIToUTF16(kUsername));
  base::MockCallback<PostSaveCompromisedHelper::BubbleCallback> callback;
  EXPECT_CALL(callback, Run(BubbleType::kNoBubble, 0));
  EXPECT_CALL(*store(), GetAllCompromisedCredentialsImpl);
  helper.AnalyzeLeakedCredentials(store(), prefs(), callback.Get());
  WaitForPasswordStore();
  EXPECT_EQ(BubbleType::kNoBubble, helper.bubble_type());
  EXPECT_EQ(0u, helper.compromised_count());
}

TEST_F(PostSaveCompromisedHelperTest, RandomSite_FullStore) {
  PostSaveCompromisedHelper helper({}, base::ASCIIToUTF16(kUsername));
  base::MockCallback<PostSaveCompromisedHelper::BubbleCallback> callback;
  EXPECT_CALL(callback, Run(BubbleType::kUnsafeState, 1));
  std::vector<CompromisedCredentials> saved = {CreateCompromised(kUsername2)};
  EXPECT_CALL(*store(), GetAllCompromisedCredentialsImpl)
      .WillOnce(Return(saved));
  helper.AnalyzeLeakedCredentials(store(), prefs(), callback.Get());
  WaitForPasswordStore();
  EXPECT_EQ(BubbleType::kUnsafeState, helper.bubble_type());
  EXPECT_EQ(1u, helper.compromised_count());
}

TEST_F(PostSaveCompromisedHelperTest, CompromisedSite_ItemStayed) {
  std::vector<CompromisedCredentials> saved = {CreateCompromised(kUsername),
                                               CreateCompromised(kUsername2)};
  PostSaveCompromisedHelper helper({saved}, base::ASCIIToUTF16(kUsername));
  base::MockCallback<PostSaveCompromisedHelper::BubbleCallback> callback;
  EXPECT_CALL(callback, Run(BubbleType::kUnsafeState, 2));
  EXPECT_CALL(*store(), GetAllCompromisedCredentialsImpl)
      .WillOnce(Return(saved));
  helper.AnalyzeLeakedCredentials(store(), prefs(), callback.Get());
  WaitForPasswordStore();
  EXPECT_EQ(BubbleType::kUnsafeState, helper.bubble_type());
  EXPECT_EQ(2u, helper.compromised_count());
}

TEST_F(PostSaveCompromisedHelperTest, CompromisedSite_ItemGone) {
  std::vector<CompromisedCredentials> saved = {CreateCompromised(kUsername),
                                               CreateCompromised(kUsername2)};
  PostSaveCompromisedHelper helper({saved}, base::ASCIIToUTF16(kUsername));
  base::MockCallback<PostSaveCompromisedHelper::BubbleCallback> callback;
  EXPECT_CALL(callback, Run(BubbleType::kPasswordUpdatedWithMoreToFix, 1));
  saved = {CreateCompromised(kUsername2)};
  EXPECT_CALL(*store(), GetAllCompromisedCredentialsImpl)
      .WillOnce(Return(saved));
  helper.AnalyzeLeakedCredentials(store(), prefs(), callback.Get());
  WaitForPasswordStore();
  EXPECT_EQ(BubbleType::kPasswordUpdatedWithMoreToFix, helper.bubble_type());
  EXPECT_EQ(1u, helper.compromised_count());
}

TEST_F(PostSaveCompromisedHelperTest, FixedLast_BulkCheckNeverDone) {
  std::vector<CompromisedCredentials> saved = {CreateCompromised(kUsername)};
  PostSaveCompromisedHelper helper({saved}, base::ASCIIToUTF16(kUsername));
  base::MockCallback<PostSaveCompromisedHelper::BubbleCallback> callback;
  EXPECT_CALL(callback, Run(BubbleType::kNoBubble, 0));
  saved = {};
  EXPECT_CALL(*store(), GetAllCompromisedCredentialsImpl)
      .WillOnce(Return(saved));
  helper.AnalyzeLeakedCredentials(store(), prefs(), callback.Get());
  WaitForPasswordStore();
  EXPECT_EQ(BubbleType::kNoBubble, helper.bubble_type());
  EXPECT_EQ(0u, helper.compromised_count());
}

TEST_F(PostSaveCompromisedHelperTest, FixedLast_BulkCheckDoneLongAgo) {
  prefs()->SetDouble(
      kLastTimePasswordCheckCompleted,
      (base::Time::Now() - base::TimeDelta::FromDays(5)).ToDoubleT());
  std::vector<CompromisedCredentials> saved = {CreateCompromised(kUsername)};
  PostSaveCompromisedHelper helper({saved}, base::ASCIIToUTF16(kUsername));
  base::MockCallback<PostSaveCompromisedHelper::BubbleCallback> callback;
  EXPECT_CALL(callback, Run(BubbleType::kNoBubble, 0));
  saved = {};
  EXPECT_CALL(*store(), GetAllCompromisedCredentialsImpl)
      .WillOnce(Return(saved));
  helper.AnalyzeLeakedCredentials(store(), prefs(), callback.Get());
  WaitForPasswordStore();
  EXPECT_EQ(BubbleType::kNoBubble, helper.bubble_type());
  EXPECT_EQ(0u, helper.compromised_count());
}

TEST_F(PostSaveCompromisedHelperTest, FixedLast_BulkCheckDoneRecently) {
  prefs()->SetDouble(
      kLastTimePasswordCheckCompleted,
      (base::Time::Now() - base::TimeDelta::FromMinutes(1)).ToDoubleT());
  std::vector<CompromisedCredentials> saved = {CreateCompromised(kUsername)};
  PostSaveCompromisedHelper helper({saved}, base::ASCIIToUTF16(kUsername));
  base::MockCallback<PostSaveCompromisedHelper::BubbleCallback> callback;
  EXPECT_CALL(callback, Run(BubbleType::kPasswordUpdatedSafeState, 0));
  saved = {};
  EXPECT_CALL(*store(), GetAllCompromisedCredentialsImpl)
      .WillOnce(Return(saved));
  helper.AnalyzeLeakedCredentials(store(), prefs(), callback.Get());
  WaitForPasswordStore();
  EXPECT_EQ(BubbleType::kPasswordUpdatedSafeState, helper.bubble_type());
  EXPECT_EQ(0u, helper.compromised_count());
}

}  // namespace password_manager
