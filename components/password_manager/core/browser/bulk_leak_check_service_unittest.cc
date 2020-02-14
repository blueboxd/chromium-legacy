// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <algorithm>

#include "components/password_manager/core/browser/bulk_leak_check_service.h"

#include "base/strings/utf_string_conversions.h"
#include "base/test/task_environment.h"
#include "components/password_manager/core/browser/leak_detection/mock_leak_detection_check_factory.h"
#include "components/signin/public/identity_manager/identity_test_environment.h"
#include "services/network/test/test_shared_url_loader_factory.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace password_manager {
namespace {

using ::testing::ByMove;
using ::testing::DoAll;
using ::testing::Return;
using ::testing::SaveArg;
using ::testing::StrictMock;
using ::testing::WithArg;

constexpr char kUsername[] = "user";
constexpr char kPassword[] = "password123";

MATCHER_P(CredentialsAre, credentials, "") {
  return std::equal(arg.begin(), arg.end(), credentials.get().begin(),
                    credentials.get().end(),
                    [](const auto& lhs, const auto& rhs) {
                      return lhs.username() == rhs.username() &&
                             lhs.password() == rhs.password();
                    });
  ;
}

MATCHER_P(CredentialIs, credential, "") {
  return arg.username() == credential.get().username() &&
         arg.password() == credential.get().password();
}

LeakCheckCredential TestCredential() {
  return LeakCheckCredential(base::ASCIIToUTF16(kUsername),
                             base::ASCIIToUTF16(kPassword));
}

std::vector<LeakCheckCredential> TestCredentials() {
  std::vector<LeakCheckCredential> result;
  result.push_back(TestCredential());
  return result;
}

class MockBulkLeakCheck : public BulkLeakCheck {
 public:
  MOCK_METHOD(void,
              CheckCredentials,
              (std::vector<LeakCheckCredential> credentials),
              (override));
  MOCK_METHOD(size_t, GetPendingChecksCount, (), (const override));
};

class MockObserver : public BulkLeakCheckService::Observer {
 public:
  MOCK_METHOD(void,
              OnStateChanged,
              (BulkLeakCheckService::State state, size_t pending_credentials),
              (override));
  MOCK_METHOD(void,
              OnLeakFound,
              (const LeakCheckCredential& credential),
              (override));
};

class BulkLeakCheckServiceTest : public testing::Test {
 public:
  BulkLeakCheckServiceTest()
      : service_(identity_test_env_.identity_manager(),
                 base::MakeRefCounted<network::TestSharedURLLoaderFactory>()) {
    auto factory = std::make_unique<MockLeakDetectionCheckFactory>();
    factory_ = factory.get();
    service_.set_leak_factory(std::move(factory));
  }
  ~BulkLeakCheckServiceTest() override { service_.Shutdown(); }

  BulkLeakCheckService& service() { return service_; }
  MockLeakDetectionCheckFactory& factory() { return *factory_; }

 private:
  base::test::TaskEnvironment task_env_;
  signin::IdentityTestEnvironment identity_test_env_;
  BulkLeakCheckService service_;
  MockLeakDetectionCheckFactory* factory_;
};

TEST_F(BulkLeakCheckServiceTest, OnCreation) {
  EXPECT_EQ(0u, service().GetPendingChecksCount());
  EXPECT_EQ(BulkLeakCheckService::State::kIdle, service().state());
}

TEST_F(BulkLeakCheckServiceTest, Running) {
  StrictMock<MockObserver> observer;
  service().AddObserver(&observer);

  auto leak_check = std::make_unique<MockBulkLeakCheck>();
  const std::vector<LeakCheckCredential> credentials = TestCredentials();
  EXPECT_CALL(*leak_check,
              CheckCredentials(CredentialsAre(std::cref(credentials))));
  EXPECT_CALL(*leak_check, GetPendingChecksCount).WillRepeatedly(Return(10));
  EXPECT_CALL(factory(), TryCreateBulkLeakCheck)
      .WillOnce(Return(ByMove(std::move(leak_check))));
  EXPECT_CALL(observer,
              OnStateChanged(BulkLeakCheckService::State::kRunning, 10));
  service().CheckUsernamePasswordPairs(TestCredentials());

  EXPECT_EQ(BulkLeakCheckService::State::kRunning, service().state());
  EXPECT_EQ(10u, service().GetPendingChecksCount());
}

TEST_F(BulkLeakCheckServiceTest, AppendRunning) {
  StrictMock<MockObserver> observer;
  service().AddObserver(&observer);

  auto leak_check = std::make_unique<MockBulkLeakCheck>();
  MockBulkLeakCheck* weak_leak_check = leak_check.get();
  EXPECT_CALL(factory(), TryCreateBulkLeakCheck)
      .WillOnce(Return(ByMove(std::move(leak_check))));
  EXPECT_CALL(*weak_leak_check, CheckCredentials);
  EXPECT_CALL(*weak_leak_check, GetPendingChecksCount)
      .WillRepeatedly(Return(10));
  EXPECT_CALL(observer,
              OnStateChanged(BulkLeakCheckService::State::kRunning, 10));
  service().CheckUsernamePasswordPairs(TestCredentials());

  const std::vector<LeakCheckCredential> credentials = TestCredentials();
  EXPECT_CALL(*weak_leak_check,
              CheckCredentials(CredentialsAre(std::cref(credentials))));
  EXPECT_CALL(*weak_leak_check, GetPendingChecksCount)
      .WillRepeatedly(Return(20));
  EXPECT_CALL(observer,
              OnStateChanged(BulkLeakCheckService::State::kRunning, 20));
  service().CheckUsernamePasswordPairs(TestCredentials());

  EXPECT_EQ(BulkLeakCheckService::State::kRunning, service().state());
  EXPECT_EQ(20u, service().GetPendingChecksCount());
}

TEST_F(BulkLeakCheckServiceTest, FailedToCreateCheck) {
  StrictMock<MockObserver> observer;
  service().AddObserver(&observer);

  EXPECT_CALL(factory(), TryCreateBulkLeakCheck)
      .WillOnce(Return(ByMove(nullptr)));
  service().CheckUsernamePasswordPairs(TestCredentials());

  EXPECT_EQ(BulkLeakCheckService::State::kIdle, service().state());
  EXPECT_EQ(0u, service().GetPendingChecksCount());
}

TEST_F(BulkLeakCheckServiceTest, FailedToCreateCheckWithError) {
  StrictMock<MockObserver> observer;
  service().AddObserver(&observer);

  EXPECT_CALL(factory(), TryCreateBulkLeakCheck)
      .WillOnce(WithArg<0>([](BulkLeakCheckDelegateInterface* delegate) {
        delegate->OnError(LeakDetectionError::kNotSignIn);
        return nullptr;
      }));
  EXPECT_CALL(observer,
              OnStateChanged(BulkLeakCheckService::State::kSignedOut, 0));
  service().CheckUsernamePasswordPairs(TestCredentials());

  EXPECT_EQ(BulkLeakCheckService::State::kSignedOut, service().state());
  EXPECT_EQ(0u, service().GetPendingChecksCount());
}

TEST_F(BulkLeakCheckServiceTest, CancelNothing) {
  StrictMock<MockObserver> observer;
  service().AddObserver(&observer);

  service().Cancel();

  EXPECT_EQ(BulkLeakCheckService::State::kIdle, service().state());
  EXPECT_EQ(0u, service().GetPendingChecksCount());
}

TEST_F(BulkLeakCheckServiceTest, CancelSomething) {
  auto leak_check = std::make_unique<MockBulkLeakCheck>();
  EXPECT_CALL(*leak_check, CheckCredentials);
  EXPECT_CALL(*leak_check, GetPendingChecksCount).WillRepeatedly(Return(10));
  EXPECT_CALL(factory(), TryCreateBulkLeakCheck)
      .WillOnce(Return(ByMove(std::move(leak_check))));
  service().CheckUsernamePasswordPairs(TestCredentials());

  StrictMock<MockObserver> observer;
  service().AddObserver(&observer);
  EXPECT_CALL(observer, OnStateChanged(BulkLeakCheckService::State::kIdle, 0));
  service().Cancel();

  EXPECT_EQ(BulkLeakCheckService::State::kIdle, service().state());
  EXPECT_EQ(0u, service().GetPendingChecksCount());
}

TEST_F(BulkLeakCheckServiceTest, NotifyAboutLeak) {
  auto leak_check = std::make_unique<MockBulkLeakCheck>();
  EXPECT_CALL(*leak_check, CheckCredentials);
  EXPECT_CALL(*leak_check, GetPendingChecksCount).WillRepeatedly(Return(10));
  BulkLeakCheckDelegateInterface* delegate = nullptr;
  EXPECT_CALL(factory(), TryCreateBulkLeakCheck)
      .WillOnce(
          DoAll(SaveArg<0>(&delegate), Return(ByMove(std::move(leak_check)))));
  service().CheckUsernamePasswordPairs(TestCredentials());

  StrictMock<MockObserver> observer;
  service().AddObserver(&observer);
  delegate->OnFinishedCredential(
      LeakCheckCredential(base::ASCIIToUTF16(kUsername),
                          base::ASCIIToUTF16("nfidog8h894e5hn")),
      IsLeaked(false));
  LeakCheckCredential leaked_credential = TestCredential();
  EXPECT_CALL(observer,
              OnLeakFound(CredentialIs(std::cref(leaked_credential))));
  delegate->OnFinishedCredential(TestCredential(), IsLeaked(true));
}

TEST_F(BulkLeakCheckServiceTest, CheckFinished) {
  auto leak_check = std::make_unique<MockBulkLeakCheck>();
  MockBulkLeakCheck* weak_leak_check = leak_check.get();
  EXPECT_CALL(*leak_check, CheckCredentials);
  EXPECT_CALL(*leak_check, GetPendingChecksCount).WillRepeatedly(Return(10));
  BulkLeakCheckDelegateInterface* delegate = nullptr;
  EXPECT_CALL(factory(), TryCreateBulkLeakCheck)
      .WillOnce(
          DoAll(SaveArg<0>(&delegate), Return(ByMove(std::move(leak_check)))));
  service().CheckUsernamePasswordPairs(TestCredentials());

  StrictMock<MockObserver> observer;
  service().AddObserver(&observer);
  EXPECT_CALL(*weak_leak_check, GetPendingChecksCount)
      .WillRepeatedly(Return(0));
  EXPECT_CALL(observer, OnStateChanged(BulkLeakCheckService::State::kIdle, 0));
  delegate->OnFinishedCredential(TestCredential(), IsLeaked(false));

  EXPECT_EQ(BulkLeakCheckService::State::kIdle, service().state());
  EXPECT_EQ(0u, service().GetPendingChecksCount());
}

TEST_F(BulkLeakCheckServiceTest, CheckFinishedWithLeakedCredential) {
  auto leak_check = std::make_unique<MockBulkLeakCheck>();
  MockBulkLeakCheck* weak_leak_check = leak_check.get();
  EXPECT_CALL(*leak_check, CheckCredentials);
  EXPECT_CALL(*leak_check, GetPendingChecksCount).WillRepeatedly(Return(10));
  BulkLeakCheckDelegateInterface* delegate = nullptr;
  EXPECT_CALL(factory(), TryCreateBulkLeakCheck)
      .WillOnce(
          DoAll(SaveArg<0>(&delegate), Return(ByMove(std::move(leak_check)))));
  service().CheckUsernamePasswordPairs(TestCredentials());

  StrictMock<MockObserver> observer;
  service().AddObserver(&observer);
  EXPECT_CALL(*weak_leak_check, GetPendingChecksCount)
      .WillRepeatedly(Return(0));
  LeakCheckCredential leaked_credential = TestCredential();
  {
    ::testing::InSequence s;
    EXPECT_CALL(observer,
                OnLeakFound(CredentialIs(std::cref(leaked_credential))));
    EXPECT_CALL(observer,
                OnStateChanged(BulkLeakCheckService::State::kIdle, 0));
  }
  delegate->OnFinishedCredential(TestCredential(), IsLeaked(true));

  EXPECT_EQ(BulkLeakCheckService::State::kIdle, service().state());
  EXPECT_EQ(0u, service().GetPendingChecksCount());
}

TEST_F(BulkLeakCheckServiceTest, CheckFinishedWithError) {
  auto leak_check = std::make_unique<MockBulkLeakCheck>();
  EXPECT_CALL(*leak_check, CheckCredentials);
  EXPECT_CALL(*leak_check, GetPendingChecksCount).WillRepeatedly(Return(10));
  BulkLeakCheckDelegateInterface* delegate = nullptr;
  EXPECT_CALL(factory(), TryCreateBulkLeakCheck)
      .WillOnce(
          DoAll(SaveArg<0>(&delegate), Return(ByMove(std::move(leak_check)))));
  service().CheckUsernamePasswordPairs(TestCredentials());

  StrictMock<MockObserver> observer;
  service().AddObserver(&observer);
  EXPECT_CALL(observer,
              OnStateChanged(BulkLeakCheckService::State::kServiceError, 0));
  delegate->OnError(LeakDetectionError::kInvalidServerResponse);

  EXPECT_EQ(BulkLeakCheckService::State::kServiceError, service().state());
  EXPECT_EQ(0u, service().GetPendingChecksCount());
}

}  // namespace
}  // namespace password_manager
