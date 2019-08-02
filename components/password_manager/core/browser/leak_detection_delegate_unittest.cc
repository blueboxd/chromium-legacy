// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <utility>

#include "components/password_manager/core/browser/leak_detection_delegate.h"

#include "base/strings/utf_string_conversions.h"
#include "components/password_manager/core/browser/leak_detection/leak_detection_check.h"
#include "components/password_manager/core/browser/stub_password_manager_client.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace password_manager {
namespace {

using base::ASCIIToUTF16;
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
};

class MockLeakDetectionCheck : public LeakDetectionCheck {
 public:
  MOCK_METHOD3(Start,
               void(const GURL&, base::StringPiece16, base::StringPiece16));
};

class MockLeakDetectionRequestFactory : public LeakDetectionRequestFactory {
 public:
  MOCK_CONST_METHOD2(
      TryCreateLeakCheck,
      std::unique_ptr<LeakDetectionCheck>(LeakDetectionDelegateInterface*,
                                          signin::IdentityManager*));
};

}  // namespace

class LeakDetectionDelegateTest : public testing::Test {
 public:
  LeakDetectionDelegateTest() : delegate_(&client_) {
    auto mock_factory = std::make_unique<
        testing::StrictMock<MockLeakDetectionRequestFactory>>();
    mock_factory_ = mock_factory.get();
    delegate_.set_leak_factory(std::move(mock_factory));
  }
  ~LeakDetectionDelegateTest() override = default;

  MockPasswordManagerClient& client() { return client_; }
  MockLeakDetectionRequestFactory& factory() { return *mock_factory_; }
  LeakDetectionDelegate& delegate() { return delegate_; }

 private:
  MockPasswordManagerClient client_;
  MockLeakDetectionRequestFactory* mock_factory_ = nullptr;
  LeakDetectionDelegate delegate_;
};

TEST_F(LeakDetectionDelegateTest, InIncognito) {
  const autofill::PasswordForm form = CreateTestForm();
  EXPECT_CALL(client(), IsIncognito).WillOnce(Return(true));
  EXPECT_CALL(factory(), TryCreateLeakCheck).Times(0);
  delegate().StartLeakCheck(form);

  EXPECT_FALSE(delegate().leak_check());
}

TEST_F(LeakDetectionDelegateTest, StartCheck) {
  const autofill::PasswordForm form = CreateTestForm();
  EXPECT_CALL(client(), IsIncognito).WillOnce(Return(false));
  auto check_instance = std::make_unique<MockLeakDetectionCheck>();
  EXPECT_CALL(*check_instance, Start(form.origin, Eq(form.username_value),
                                     Eq(form.password_value)));
  EXPECT_CALL(factory(), TryCreateLeakCheck)
      .WillOnce(Return(ByMove(std::move(check_instance))));
  delegate().StartLeakCheck(form);

  EXPECT_TRUE(delegate().leak_check());
}

}  // namespace password_manager
