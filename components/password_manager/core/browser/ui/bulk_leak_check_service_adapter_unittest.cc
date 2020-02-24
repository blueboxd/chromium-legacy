// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/password_manager/core/browser/ui/bulk_leak_check_service_adapter.h"

#include <tuple>
#include <vector>

#include "base/containers/span.h"
#include "base/memory/ptr_util.h"
#include "base/strings/string_piece_forward.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/gmock_move_support.h"
#include "base/test/task_environment.h"
#include "components/autofill/core/common/password_form.h"
#include "components/password_manager/core/browser/bulk_leak_check_service.h"
#include "components/password_manager/core/browser/leak_detection/bulk_leak_check.h"
#include "components/password_manager/core/browser/leak_detection/leak_detection_check_factory.h"
#include "components/password_manager/core/browser/leak_detection/mock_leak_detection_check_factory.h"
#include "components/password_manager/core/browser/test_password_store.h"
#include "components/password_manager/core/browser/ui/saved_passwords_presenter.h"
#include "components/signin/public/identity_manager/identity_test_environment.h"
#include "components/sync/model/syncable_service.h"
#include "services/network/test/test_shared_url_loader_factory.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace password_manager {
namespace {

constexpr char kExampleCom[] = "https://example.com";
constexpr char kExampleOrg[] = "https://example.org";

constexpr char kUsername1[] = "alice";
constexpr char kUsername2[] = "bob";

constexpr char kPassword1[] = "f00b4r";
constexpr char kPassword2[] = "s3cr3t";

using autofill::PasswordForm;
using ::testing::ByMove;
using ::testing::NiceMock;
using ::testing::Return;

MATCHER_P(CredentialsAre, credentials, "") {
  return std::equal(arg.begin(), arg.end(), credentials.get().begin(),
                    credentials.get().end(),
                    [](const auto& lhs, const auto& rhs) {
                      return lhs.username() == rhs.username() &&
                             lhs.password() == rhs.password();
                    });
}

MATCHER_P(SavedPasswordsAre, passwords, "") {
  return std::equal(arg.begin(), arg.end(), passwords.begin(), passwords.end(),
                    [](const auto& lhs, const auto& rhs) {
                      return lhs.signon_realm == rhs.signon_realm &&
                             lhs.username_value == rhs.username_value &&
                             lhs.password_value == rhs.password_value;
                    });
}

PasswordForm MakeSavedPassword(base::StringPiece signon_realm,
                               base::StringPiece username,
                               base::StringPiece password) {
  PasswordForm form;
  form.signon_realm = std::string(signon_realm);
  form.username_value = base::ASCIIToUTF16(username);
  form.password_value = base::ASCIIToUTF16(password);
  return form;
}

LeakCheckCredential MakeLeakCheckCredential(base::StringPiece username,
                                            base::StringPiece password) {
  return LeakCheckCredential(base::ASCIIToUTF16(username),
                             base::ASCIIToUTF16(password));
}

struct MockBulkLeakCheck : BulkLeakCheck {
  MOCK_METHOD(void,
              CheckCredentials,
              (std::vector<LeakCheckCredential> credentials),
              (override));
  MOCK_METHOD(size_t, GetPendingChecksCount, (), (const override));
};

using NiceMockBulkLeakCheck = ::testing::NiceMock<MockBulkLeakCheck>;

class BulkLeakCheckServiceAdapterTest : public ::testing::Test {
 public:
  BulkLeakCheckServiceAdapterTest() {
    auto factory = std::make_unique<MockLeakDetectionCheckFactory>();
    factory_ = factory.get();
    service_.set_leak_factory(std::move(factory));
    store_->Init(syncer::SyncableService::StartSyncFlare(),
                 /*prefs=*/nullptr);
  }

  ~BulkLeakCheckServiceAdapterTest() override {
    store_->ShutdownOnUIThread();
    task_env_.RunUntilIdle();
  }

  TestPasswordStore& store() { return *store_; }
  SavedPasswordsPresenter& presenter() { return presenter_; }
  MockLeakDetectionCheckFactory& factory() { return *factory_; }
  BulkLeakCheckServiceAdapter& adapter() { return adapter_; }

  void RunUntilIdle() { task_env_.RunUntilIdle(); }

 private:
  base::test::TaskEnvironment task_env_;
  signin::IdentityTestEnvironment identity_test_env_;
  scoped_refptr<TestPasswordStore> store_ =
      base::MakeRefCounted<TestPasswordStore>();
  SavedPasswordsPresenter presenter_{store_};
  BulkLeakCheckService service_{
      identity_test_env_.identity_manager(),
      base::MakeRefCounted<network::TestSharedURLLoaderFactory>()};
  MockLeakDetectionCheckFactory* factory_ = nullptr;
  BulkLeakCheckServiceAdapter adapter_{&presenter_, &service_};
};

}  // namespace

TEST_F(BulkLeakCheckServiceAdapterTest, OnCreation) {
  EXPECT_EQ(0u, adapter().GetPendingChecksCount());
  EXPECT_EQ(BulkLeakCheckService::State::kIdle,
            adapter().GetBulkLeakCheckState());
}

// Checks that starting a leak check correctly transforms the list of saved
// passwords into LeakCheckCredentials and attaches the underlying password
// forms as user data.
TEST_F(BulkLeakCheckServiceAdapterTest, StartBulkLeakCheck) {
  std::vector<PasswordForm> passwords = {
      MakeSavedPassword(kExampleCom, kUsername1, kPassword1),
      MakeSavedPassword(kExampleOrg, kUsername2, kPassword2)};
  store().AddLogin(passwords[0]);
  store().AddLogin(passwords[1]);
  RunUntilIdle();

  auto leak_check = std::make_unique<NiceMockBulkLeakCheck>();
  std::vector<LeakCheckCredential> credentials;
  EXPECT_CALL(*leak_check, CheckCredentials).WillOnce(MoveArg(&credentials));
  EXPECT_CALL(factory(), TryCreateBulkLeakCheck)
      .WillOnce(Return(ByMove(std::move(leak_check))));
  adapter().StartBulkLeakCheck();

  std::vector<LeakCheckCredential> expected;
  expected.push_back(MakeLeakCheckCredential(kUsername1, kPassword1));
  expected.push_back(MakeLeakCheckCredential(kUsername2, kPassword2));

  EXPECT_THAT(credentials, CredentialsAre(std::cref(expected)));

  EXPECT_THAT(static_cast<BulkLeakCheckData*>(
                  credentials[0].GetUserData(kBulkLeakCheckDataKey))
                  ->leaked_forms,
              SavedPasswordsAre(base::make_span(&passwords[0], 1)));

  EXPECT_THAT(static_cast<BulkLeakCheckData*>(
                  credentials[1].GetUserData(kBulkLeakCheckDataKey))
                  ->leaked_forms,
              SavedPasswordsAre(base::make_span(&passwords[1], 1)));
}

// Tests that multiple credentials with effectively the same username are
// correctly deduped before starting the leak check.
TEST_F(BulkLeakCheckServiceAdapterTest, StartBulkLeakCheckDedupes) {
  std::vector<PasswordForm> passwords = {
      MakeSavedPassword(kExampleCom, "alice", kPassword1),
      MakeSavedPassword(kExampleCom, "ALICE", kPassword1),
      MakeSavedPassword(kExampleCom, "Alice@example.com", kPassword1)};

  store().AddLogin(passwords[0]);
  store().AddLogin(passwords[1]);
  store().AddLogin(passwords[2]);
  RunUntilIdle();

  auto leak_check = std::make_unique<NiceMockBulkLeakCheck>();
  std::vector<LeakCheckCredential> credentials;
  EXPECT_CALL(*leak_check, CheckCredentials).WillOnce(MoveArg(&credentials));
  EXPECT_CALL(factory(), TryCreateBulkLeakCheck)
      .WillOnce(Return(ByMove(std::move(leak_check))));
  adapter().StartBulkLeakCheck();

  std::vector<LeakCheckCredential> expected;
  expected.push_back(MakeLeakCheckCredential("alice", kPassword1));
  EXPECT_THAT(credentials, CredentialsAre(std::cref(expected)));

  EXPECT_THAT(static_cast<BulkLeakCheckData*>(
                  credentials[0].GetUserData(kBulkLeakCheckDataKey))
                  ->leaked_forms,
              SavedPasswordsAre(passwords));
}

// Checks that trying to start a leak check when another check is already
// running does nothing and returns false to the caller.
TEST_F(BulkLeakCheckServiceAdapterTest, MultipleStarts) {
  auto leak_check = std::make_unique<NiceMockBulkLeakCheck>();
  auto& leak_check_ref = *leak_check;
  EXPECT_CALL(leak_check_ref, CheckCredentials);
  EXPECT_CALL(factory(), TryCreateBulkLeakCheck)
      .WillOnce(Return(ByMove(std::move(leak_check))));
  EXPECT_TRUE(adapter().StartBulkLeakCheck());

  EXPECT_CALL(leak_check_ref, CheckCredentials).Times(0);
  EXPECT_FALSE(adapter().StartBulkLeakCheck());
}

// Checks that stopping the leak check correctly resets the state of the bulk
// leak check.
TEST_F(BulkLeakCheckServiceAdapterTest, StopBulkLeakCheck) {
  auto leak_check = std::make_unique<NiceMockBulkLeakCheck>();
  EXPECT_CALL(*leak_check, CheckCredentials);
  EXPECT_CALL(factory(), TryCreateBulkLeakCheck)
      .WillOnce(Return(ByMove(std::move(leak_check))));
  adapter().StartBulkLeakCheck();
  EXPECT_EQ(BulkLeakCheckService::State::kRunning,
            adapter().GetBulkLeakCheckState());

  adapter().StopBulkLeakCheck();
  EXPECT_EQ(BulkLeakCheckService::State::kIdle,
            adapter().GetBulkLeakCheckState());
}

// Tests that editing a password through the presenter will result in another
// call to CheckCredentials with a corresponding change to the checked password.
TEST_F(BulkLeakCheckServiceAdapterTest, OnEdited) {
  PasswordForm password =
      MakeSavedPassword(kExampleCom, kUsername1, kPassword1);
  store().AddLogin(password);
  RunUntilIdle();

  std::vector<LeakCheckCredential> expected;
  expected.push_back(MakeLeakCheckCredential(kUsername1, kPassword2));

  auto leak_check = std::make_unique<NiceMockBulkLeakCheck>();
  EXPECT_CALL(*leak_check,
              CheckCredentials(CredentialsAre(std::cref(expected))));
  EXPECT_CALL(factory(), TryCreateBulkLeakCheck)
      .WillOnce(Return(ByMove(std::move(leak_check))));
  presenter().EditPassword(password, base::ASCIIToUTF16(kPassword2));
}

}  // namespace password_manager
