// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <utility>

#include "components/password_manager/core/browser/leak_detection_delegate.h"

#include "base/strings/utf_string_conversions.h"
#include "base/test/metrics/histogram_tester.h"
#include "components/password_manager/core/browser/leak_detection/leak_detection_check.h"
#include "components/password_manager/core/browser/stub_password_manager_client.h"
#include "components/password_manager/core/common/password_manager_pref_names.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/pref_service.h"
#include "components/prefs/testing_pref_service.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace password_manager {
namespace {

using base::ASCIIToUTF16;
using testing::_;
using testing::ByMove;
using testing::Eq;
using testing::Return;

autofill::PasswordForm CreateTestForm() {
  autofill::PasswordForm form;
  form.origin = GURL("http://www.example.com/a/LoginAuth");
  form.username_value = ASCIIToUTF16("Adam");
  form.password_value = ASCIIToUTF16("p4ssword");
  form.signon_realm = "http://www.example.com/";
  return form;
}

class MockPasswordManagerClient : public StubPasswordManagerClient {
 public:
  MockPasswordManagerClient() = default;
  ~MockPasswordManagerClient() override = default;

  MOCK_CONST_METHOD0(IsIncognito, bool());
  MOCK_CONST_METHOD0(GetPrefs, PrefService*());
  MOCK_METHOD2(NotifyUserCredentialsWereLeaked,
               void(password_manager::CredentialLeakType, const GURL&));
};

class MockLeakDetectionCheck : public LeakDetectionCheck {
 public:
  MOCK_METHOD3(Start, void(const GURL&, base::string16, base::string16));
};

class MockLeakDetectionCheckFactory : public LeakDetectionCheckFactory {
 public:
  MOCK_CONST_METHOD3(TryCreateLeakCheck,
                     std::unique_ptr<LeakDetectionCheck>(
                         LeakDetectionDelegateInterface*,
                         signin::IdentityManager*,
                         scoped_refptr<network::SharedURLLoaderFactory>));
};

}  // namespace

class LeakDetectionDelegateTest : public testing::Test {
 public:
  LeakDetectionDelegateTest() : delegate_(&client_) {
    auto mock_factory =
        std::make_unique<testing::StrictMock<MockLeakDetectionCheckFactory>>();
    mock_factory_ = mock_factory.get();
    delegate_.set_leak_factory(std::move(mock_factory));
    prefs_ = std::make_unique<TestingPrefServiceSimple>();
    prefs_->registry()->RegisterBooleanPref(
        password_manager::prefs::kPasswordLeakDetectionEnabled, true);
    ON_CALL(client_, GetPrefs()).WillByDefault(Return(prefs_.get()));
  }
  ~LeakDetectionDelegateTest() override = default;

  MockPasswordManagerClient& client() { return client_; }
  MockLeakDetectionCheckFactory& factory() { return *mock_factory_; }
  LeakDetectionDelegate& delegate() { return delegate_; }

 protected:
  std::unique_ptr<TestingPrefServiceSimple> prefs_;

 private:
  testing::NiceMock<MockPasswordManagerClient> client_;
  MockLeakDetectionCheckFactory* mock_factory_ = nullptr;
  LeakDetectionDelegate delegate_;
};

TEST_F(LeakDetectionDelegateTest, InIncognito) {
  const autofill::PasswordForm form = CreateTestForm();
  EXPECT_CALL(client(), IsIncognito).WillOnce(Return(true));
  EXPECT_CALL(factory(), TryCreateLeakCheck).Times(0);
  delegate().StartLeakCheck(form);

  EXPECT_FALSE(delegate().leak_check());
}

TEST_F(LeakDetectionDelegateTest, PrefIsFalse) {
  const autofill::PasswordForm form = CreateTestForm();
  prefs_->SetBoolean(password_manager::prefs::kPasswordLeakDetectionEnabled,
                     false);

  EXPECT_CALL(factory(), TryCreateLeakCheck).Times(0);
  delegate().StartLeakCheck(form);

  EXPECT_FALSE(delegate().leak_check());
}

TEST_F(LeakDetectionDelegateTest, StartCheck) {
  const autofill::PasswordForm form = CreateTestForm();
  EXPECT_CALL(client(), IsIncognito).WillOnce(Return(false));
  auto check_instance = std::make_unique<MockLeakDetectionCheck>();
  EXPECT_CALL(*check_instance,
              Start(form.origin, form.username_value, form.password_value));
  EXPECT_CALL(factory(), TryCreateLeakCheck(&delegate(), _, _))
      .WillOnce(Return(ByMove(std::move(check_instance))));
  delegate().StartLeakCheck(form);

  EXPECT_TRUE(delegate().leak_check());
}

TEST_F(LeakDetectionDelegateTest, LeakDetectionDone) {
  base::HistogramTester histogram_tester;
  LeakDetectionDelegateInterface* delegate_interface = &delegate();
  const autofill::PasswordForm form = CreateTestForm();

  EXPECT_CALL(factory(), TryCreateLeakCheck)
      .WillOnce(Return(ByMove(std::make_unique<MockLeakDetectionCheck>())));
  delegate().StartLeakCheck(form);

  EXPECT_CALL(client(), NotifyUserCredentialsWereLeaked).Times(0);
  delegate_interface->OnLeakDetectionDone(
      false, form.origin, form.username_value, form.password_value);
  histogram_tester.ExpectTotalCount(
      "PasswordManager.LeakDetection.NotifyIsLeakedTime", 0);

  EXPECT_CALL(client(), NotifyUserCredentialsWereLeaked(
                            password_manager::CreateLeakTypeFromBools(
                                /*is_saved=*/false, /*is_reused=*/false,
                                /*is_syncing=*/false),
                            form.origin));
  delegate_interface->OnLeakDetectionDone(
      true, form.origin, form.username_value, form.password_value);
  histogram_tester.ExpectTotalCount(
      "PasswordManager.LeakDetection.NotifyIsLeakedTime", 1);
}

}  // namespace password_manager
