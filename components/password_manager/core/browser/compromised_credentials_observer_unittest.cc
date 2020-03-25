// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/password_manager/core/browser/compromised_credentials_observer.h"

#include "base/memory/scoped_refptr.h"
#include "base/strings/string_piece.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/task_environment.h"
#include "components/password_manager/core/browser/mock_password_store.h"
#include "components/password_manager/core/browser/password_store_change.h"
#include "components/password_manager/core/common/password_manager_features.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace password_manager {
namespace {

constexpr char kHistogramName[] =
    "PasswordManager.RemoveCompromisedCredentials";
constexpr char kSite[] = "https://example.com/path";
constexpr char kUsername[] = "peter";
constexpr char kUsernameNew[] = "ana";

autofill::PasswordForm TestForm(base::StringPiece username) {
  autofill::PasswordForm form;
  form.origin = GURL(kSite);
  form.signon_realm = form.origin.GetOrigin().spec();
  form.username_value = base::ASCIIToUTF16(username);
  form.password_value = base::ASCIIToUTF16("12345");
  return form;
}

class CompromisedCredentialsObserverTest : public testing::Test {
 public:
  CompromisedCredentialsObserverTest() {
    feature_list_.InitAndEnableFeature(features::kPasswordCheck);
    mock_store_->Init(nullptr);
    observer_.Initialize();
  }

  ~CompromisedCredentialsObserverTest() override {
    mock_store_->ShutdownOnUIThread();
  }

  void WaitForPasswordStore() { task_environment_.RunUntilIdle(); }
  MockPasswordStore& store() { return *mock_store_; }
  base::HistogramTester& histogram_tester() { return histogram_tester_; }
  PasswordStore::Observer& observer() { return observer_; }

 private:
  base::test::SingleThreadTaskEnvironment task_environment_;
  base::test::ScopedFeatureList feature_list_;
  scoped_refptr<MockPasswordStore> mock_store_ =
      base::MakeRefCounted<testing::StrictMock<MockPasswordStore>>();
  base::HistogramTester histogram_tester_;
  CompromisedCredentialsObserver observer_{mock_store_.get()};
};

TEST_F(CompromisedCredentialsObserverTest, DeletePassword) {
  const autofill::PasswordForm form = TestForm(kUsername);
  EXPECT_CALL(store(), RemoveCompromisedCredentialsImpl(
                           form.signon_realm, form.username_value,
                           RemoveCompromisedCredentialsReason::kRemove));
  observer().OnLoginsChanged(
      {PasswordStoreChange(PasswordStoreChange::REMOVE, form)});
  WaitForPasswordStore();
  histogram_tester().ExpectUniqueSample(kHistogramName,
                                        PasswordStoreChange::REMOVE, 1);
}

TEST_F(CompromisedCredentialsObserverTest, UpdateFormNoPasswordChange) {
  const autofill::PasswordForm form = TestForm(kUsername);
  EXPECT_CALL(store(), RemoveCompromisedCredentialsImpl).Times(0);
  observer().OnLoginsChanged(
      {PasswordStoreChange(PasswordStoreChange::UPDATE, form, 1000, false)});
  WaitForPasswordStore();
  histogram_tester().ExpectTotalCount(kHistogramName, 0);
}

TEST_F(CompromisedCredentialsObserverTest, UpdatePassword) {
  const autofill::PasswordForm form = TestForm(kUsername);
  EXPECT_CALL(store(), RemoveCompromisedCredentialsImpl(
                           form.signon_realm, form.username_value,
                           RemoveCompromisedCredentialsReason::kUpdate));
  observer().OnLoginsChanged(
      {PasswordStoreChange(PasswordStoreChange::UPDATE, form, 1000, true)});
  WaitForPasswordStore();
  histogram_tester().ExpectUniqueSample(kHistogramName,
                                        PasswordStoreChange::UPDATE, 1);
}

TEST_F(CompromisedCredentialsObserverTest, UpdateTwice) {
  const autofill::PasswordForm form = TestForm(kUsername);
  EXPECT_CALL(store(), RemoveCompromisedCredentialsImpl(
                           form.signon_realm, form.username_value,
                           RemoveCompromisedCredentialsReason::kUpdate));
  observer().OnLoginsChanged(
      {PasswordStoreChange(PasswordStoreChange::UPDATE, TestForm(kUsernameNew),
                           1000, false),
       PasswordStoreChange(PasswordStoreChange::UPDATE, form, 1001, true)});
  WaitForPasswordStore();
  histogram_tester().ExpectUniqueSample(kHistogramName,
                                        PasswordStoreChange::UPDATE, 1);
}

TEST_F(CompromisedCredentialsObserverTest, AddPassword) {
  const autofill::PasswordForm form = TestForm(kUsername);
  EXPECT_CALL(store(), RemoveCompromisedCredentialsImpl).Times(0);
  observer().OnLoginsChanged(
      {PasswordStoreChange(PasswordStoreChange::ADD, form)});
  WaitForPasswordStore();
  histogram_tester().ExpectTotalCount(kHistogramName, 0);
}

TEST_F(CompromisedCredentialsObserverTest, AddReplacePassword) {
  autofill::PasswordForm form = TestForm(kUsername);
  PasswordStoreChange remove(PasswordStoreChange::REMOVE, form);
  form.password_value = base::ASCIIToUTF16("new_password_12345");
  PasswordStoreChange add(PasswordStoreChange::ADD, form);
  EXPECT_CALL(store(), RemoveCompromisedCredentialsImpl(
                           form.signon_realm, form.username_value,
                           RemoveCompromisedCredentialsReason::kUpdate));
  observer().OnLoginsChanged({remove, add});
  WaitForPasswordStore();
  histogram_tester().ExpectUniqueSample(kHistogramName,
                                        PasswordStoreChange::UPDATE, 1);
}

TEST_F(CompromisedCredentialsObserverTest, UpdateWithPrimaryKey) {
  const autofill::PasswordForm old_form = TestForm(kUsername);
  PasswordStoreChange remove(PasswordStoreChange::REMOVE, old_form);
  PasswordStoreChange add(PasswordStoreChange::ADD, TestForm(kUsernameNew));
  EXPECT_CALL(store(), RemoveCompromisedCredentialsImpl(
                           old_form.signon_realm, old_form.username_value,
                           RemoveCompromisedCredentialsReason::kUpdate));
  observer().OnLoginsChanged({remove, add});
  WaitForPasswordStore();
  histogram_tester().ExpectUniqueSample(kHistogramName,
                                        PasswordStoreChange::UPDATE, 1);
}

TEST_F(CompromisedCredentialsObserverTest, UpdateWithPrimaryKey_RemoveTwice) {
  const autofill::PasswordForm old_form = TestForm(kUsername);
  PasswordStoreChange remove_old(PasswordStoreChange::REMOVE, old_form);
  const autofill::PasswordForm conflicting_new_form = TestForm(kUsernameNew);
  PasswordStoreChange remove_conflicting(PasswordStoreChange::REMOVE,
                                         conflicting_new_form);
  PasswordStoreChange add(PasswordStoreChange::ADD, TestForm(kUsernameNew));
  EXPECT_CALL(store(), RemoveCompromisedCredentialsImpl(
                           old_form.signon_realm, old_form.username_value,
                           RemoveCompromisedCredentialsReason::kUpdate));
  EXPECT_CALL(store(), RemoveCompromisedCredentialsImpl(
                           conflicting_new_form.signon_realm,
                           conflicting_new_form.username_value,
                           RemoveCompromisedCredentialsReason::kUpdate));
  observer().OnLoginsChanged({remove_old, remove_conflicting, add});
  WaitForPasswordStore();
  histogram_tester().ExpectUniqueSample(kHistogramName,
                                        PasswordStoreChange::UPDATE, 2);
}

}  // namespace
}  // namespace password_manager
