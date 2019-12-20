// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/sync/driver/sync_service_crypto.h"

#include <list>
#include <map>
#include <utility>

#include "base/bind_helpers.h"
#include "base/run_loop.h"
#include "base/test/mock_callback.h"
#include "components/sync/base/sync_prefs.h"
#include "components/sync/driver/trusted_vault_client.h"
#include "components/sync/engine/mock_sync_engine.h"
#include "components/sync/nigori/nigori.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace syncer {

namespace {

using testing::_;
using testing::Eq;

sync_pb::EncryptedData MakeEncryptedData(
    const std::string& passphrase,
    const KeyDerivationParams& derivation_params) {
  std::unique_ptr<Nigori> nigori =
      Nigori::CreateByDerivation(derivation_params, passphrase);

  std::string nigori_name;
  EXPECT_TRUE(
      nigori->Permute(Nigori::Type::Password, kNigoriKeyName, &nigori_name));

  const std::string unencrypted = "test";
  sync_pb::EncryptedData encrypted;
  encrypted.set_key_name(nigori_name);
  EXPECT_TRUE(nigori->Encrypt(unencrypted, encrypted.mutable_blob()));
  return encrypted;
}

CoreAccountInfo MakeAccountInfoWithGaia(const std::string& gaia) {
  CoreAccountInfo result;
  result.gaia = gaia;
  return result;
}

class MockCryptoSyncPrefs : public CryptoSyncPrefs {
 public:
  MockCryptoSyncPrefs() = default;
  ~MockCryptoSyncPrefs() override = default;

  MOCK_CONST_METHOD0(GetEncryptionBootstrapToken, std::string());
  MOCK_METHOD1(SetEncryptionBootstrapToken, void(const std::string&));
  MOCK_CONST_METHOD0(GetKeystoreEncryptionBootstrapToken, std::string());
  MOCK_METHOD1(SetKeystoreEncryptionBootstrapToken, void(const std::string&));
};

// Simple in-memory implementation of TrustedVaultClient.
class TestTrustedVaultClient : public TrustedVaultClient {
 public:
  TestTrustedVaultClient() = default;
  ~TestTrustedVaultClient() override = default;

  // Exposes the total number of calls to FetchKeys().
  int fetch_count() const { return fetch_count_; }

  // Mimics the completion of the next (FIFO) FetchKeys() request.
  bool CompleteFetchKeysRequest() {
    if (pending_responses_.empty()) {
      return false;
    }

    base::OnceClosure cb = std::move(pending_responses_.front());
    pending_responses_.pop_front();
    std::move(cb).Run();
    return true;
  }

  // TrustedVaultClient implementation.
  std::unique_ptr<Subscription> AddKeysChangedObserver(
      const base::RepeatingClosure& cb) override {
    return observer_list_.Add(cb);
  }

  void FetchKeys(
      const std::string& gaia_id,
      base::OnceCallback<void(const std::vector<std::string>&)> cb) override {
    ++fetch_count_;
    pending_responses_.push_back(
        base::BindOnce(std::move(cb), gaia_id_to_keys_[gaia_id]));
  }

  void StoreKeys(const std::string& gaia_id,
                 const std::vector<std::string>& keys) override {
    gaia_id_to_keys_[gaia_id] = keys;
    observer_list_.Notify();
  }

 private:
  std::map<std::string, std::vector<std::string>> gaia_id_to_keys_;
  CallbackList observer_list_;
  int fetch_count_ = 0;
  std::list<base::OnceClosure> pending_responses_;
};

class SyncServiceCryptoTest : public testing::Test {
 protected:
  SyncServiceCryptoTest()
      : crypto_(notify_observers_cb_.Get(),
                reconfigure_cb_.Get(),
                &prefs_,
                &trusted_vault_client_) {}

  ~SyncServiceCryptoTest() override = default;

  bool VerifyAndClearExpectations() {
    return testing::Mock::VerifyAndClearExpectations(&notify_observers_cb_) &&
           testing::Mock::VerifyAndClearExpectations(&notify_observers_cb_) &&
           testing::Mock::VerifyAndClearExpectations(&trusted_vault_client_) &&
           testing::Mock::VerifyAndClearExpectations(&engine_);
  }

  testing::NiceMock<base::MockCallback<base::RepeatingClosure>>
      notify_observers_cb_;
  testing::NiceMock<
      base::MockCallback<base::RepeatingCallback<void(ConfigureReason)>>>
      reconfigure_cb_;
  testing::NiceMock<MockCryptoSyncPrefs> prefs_;
  testing::NiceMock<TestTrustedVaultClient> trusted_vault_client_;
  testing::NiceMock<MockSyncEngine> engine_;
  SyncServiceCrypto crypto_;
};

TEST_F(SyncServiceCryptoTest, ShouldExposePassphraseRequired) {
  const std::string kTestPassphrase = "somepassphrase";

  crypto_.SetSyncEngine(CoreAccountInfo(), &engine_);
  ASSERT_FALSE(crypto_.IsPassphraseRequired());
  ASSERT_THAT(trusted_vault_client_.fetch_count(), Eq(0));

  // Mimic the engine determining that a passphrase is required.
  EXPECT_CALL(reconfigure_cb_, Run(CONFIGURE_REASON_CRYPTO));
  crypto_.OnPassphraseRequired(
      REASON_DECRYPTION, KeyDerivationParams::CreateForPbkdf2(),
      MakeEncryptedData(kTestPassphrase,
                        KeyDerivationParams::CreateForPbkdf2()));
  EXPECT_TRUE(crypto_.IsPassphraseRequired());
  VerifyAndClearExpectations();

  // Entering the wrong passphrase should be rejected.
  EXPECT_CALL(reconfigure_cb_, Run(_)).Times(0);
  EXPECT_CALL(engine_, SetDecryptionPassphrase(_)).Times(0);
  EXPECT_FALSE(crypto_.SetDecryptionPassphrase("wrongpassphrase"));
  EXPECT_TRUE(crypto_.IsPassphraseRequired());

  // Entering the correct passphrase should be accepted.
  EXPECT_CALL(engine_, SetDecryptionPassphrase(kTestPassphrase))
      .WillOnce([&](const std::string&) { crypto_.OnPassphraseAccepted(); });
  // The current implementation issues two reconfigurations: one immediately
  // after checking the passphase in the UI thread and a second time later when
  // the engine confirms with OnPassphraseAccepted().
  EXPECT_CALL(reconfigure_cb_, Run(CONFIGURE_REASON_CRYPTO)).Times(2);
  EXPECT_TRUE(crypto_.SetDecryptionPassphrase(kTestPassphrase));
  EXPECT_FALSE(crypto_.IsPassphraseRequired());
}

TEST_F(SyncServiceCryptoTest,
       ShouldReadValidTrustedVaultKeysFromClientBeforeInitialization) {
  const CoreAccountInfo kSyncingAccount =
      MakeAccountInfoWithGaia("syncingaccount");
  const std::vector<std::string> kFetchedKeys = {"key1"};

  EXPECT_CALL(reconfigure_cb_, Run(_)).Times(0);
  ASSERT_FALSE(crypto_.IsTrustedVaultKeyRequired());

  // OnTrustedVaultKeyRequired() called during initialization of the sync
  // engine (i.e. before SetSyncEngine()).
  crypto_.OnTrustedVaultKeyRequired();

  trusted_vault_client_.StoreKeys(kSyncingAccount.gaia, kFetchedKeys);

  // Trusted vault keys should be fetched only after the engine initialization
  // is completed.
  ASSERT_THAT(trusted_vault_client_.fetch_count(), Eq(0));
  crypto_.SetSyncEngine(kSyncingAccount, &engine_);

  // While there is an ongoing fetch, there should be no user action required.
  EXPECT_THAT(trusted_vault_client_.fetch_count(), Eq(1));
  EXPECT_FALSE(crypto_.IsTrustedVaultKeyRequired());

  base::OnceClosure add_keys_cb;
  EXPECT_CALL(engine_, AddTrustedVaultDecryptionKeys(kFetchedKeys, _))
      .WillOnce(
          [&](const std::vector<std::string>& keys, base::OnceClosure done_cb) {
            add_keys_cb = std::move(done_cb);
          });

  // Mimic completion of the fetch.
  ASSERT_TRUE(trusted_vault_client_.CompleteFetchKeysRequest());
  ASSERT_TRUE(add_keys_cb);
  EXPECT_FALSE(crypto_.IsTrustedVaultKeyRequired());

  // Mimic completion of the engine.
  EXPECT_CALL(reconfigure_cb_, Run(CONFIGURE_REASON_CRYPTO));
  crypto_.OnTrustedVaultKeyAccepted();
  std::move(add_keys_cb).Run();
  EXPECT_FALSE(crypto_.IsTrustedVaultKeyRequired());
}

TEST_F(SyncServiceCryptoTest,
       ShouldReadValidTrustedVaultKeysFromClientAfterInitialization) {
  const CoreAccountInfo kSyncingAccount =
      MakeAccountInfoWithGaia("syncingaccount");
  const std::vector<std::string> kFetchedKeys = {"key1"};

  EXPECT_CALL(reconfigure_cb_, Run(_)).Times(0);
  ASSERT_FALSE(crypto_.IsTrustedVaultKeyRequired());

  trusted_vault_client_.StoreKeys(kSyncingAccount.gaia, kFetchedKeys);

  // Mimic the engine determining that trusted vault keys are required.
  crypto_.SetSyncEngine(kSyncingAccount, &engine_);
  ASSERT_THAT(trusted_vault_client_.fetch_count(), Eq(0));

  crypto_.OnTrustedVaultKeyRequired();

  // While there is an ongoing fetch, there should be no user action required.
  EXPECT_THAT(trusted_vault_client_.fetch_count(), Eq(1));
  EXPECT_FALSE(crypto_.IsTrustedVaultKeyRequired());

  base::OnceClosure add_keys_cb;
  EXPECT_CALL(engine_, AddTrustedVaultDecryptionKeys(kFetchedKeys, _))
      .WillOnce(
          [&](const std::vector<std::string>& keys, base::OnceClosure done_cb) {
            add_keys_cb = std::move(done_cb);
          });

  // Mimic completion of the fetch.
  ASSERT_TRUE(trusted_vault_client_.CompleteFetchKeysRequest());
  ASSERT_TRUE(add_keys_cb);
  EXPECT_FALSE(crypto_.IsTrustedVaultKeyRequired());

  // Mimic completion of the engine.
  EXPECT_CALL(reconfigure_cb_, Run(CONFIGURE_REASON_CRYPTO));
  crypto_.OnTrustedVaultKeyAccepted();
  std::move(add_keys_cb).Run();
  EXPECT_FALSE(crypto_.IsTrustedVaultKeyRequired());
}

TEST_F(SyncServiceCryptoTest, ShouldReadInvalidTrustedVaultKeysFromClient) {
  const CoreAccountInfo kSyncingAccount =
      MakeAccountInfoWithGaia("syncingaccount");
  const std::vector<std::string> kFetchedKeys = {"key1"};

  ASSERT_FALSE(crypto_.IsTrustedVaultKeyRequired());

  trusted_vault_client_.StoreKeys(kSyncingAccount.gaia, kFetchedKeys);

  // Mimic the engine determining that trusted vault keys are required.
  crypto_.SetSyncEngine(kSyncingAccount, &engine_);
  ASSERT_THAT(trusted_vault_client_.fetch_count(), Eq(0));

  crypto_.OnTrustedVaultKeyRequired();

  // While there is an ongoing fetch, there should be no user action required.
  EXPECT_THAT(trusted_vault_client_.fetch_count(), Eq(1));
  EXPECT_FALSE(crypto_.IsTrustedVaultKeyRequired());

  base::OnceClosure add_keys_cb;
  EXPECT_CALL(engine_, AddTrustedVaultDecryptionKeys(kFetchedKeys, _))
      .WillOnce(
          [&](const std::vector<std::string>& keys, base::OnceClosure done_cb) {
            add_keys_cb = std::move(done_cb);
          });

  // Mimic completion of the client.
  ASSERT_TRUE(trusted_vault_client_.CompleteFetchKeysRequest());
  ASSERT_TRUE(add_keys_cb);
  EXPECT_FALSE(crypto_.IsTrustedVaultKeyRequired());

  // Mimic completion of the engine, without OnTrustedVaultKeyAccepted().
  EXPECT_CALL(reconfigure_cb_, Run(CONFIGURE_REASON_CRYPTO));
  std::move(add_keys_cb).Run();
  EXPECT_TRUE(crypto_.IsTrustedVaultKeyRequired());
}

// Similar to ShouldReadInvalidTrustedVaultKeysFromClient: the vault
// initially has no valid keys, leading to IsTrustedVaultKeyRequired().
// Later, the vault gets populated with the keys, which should trigger
// a fetch and eventually resolve the encryption issue.
TEST_F(SyncServiceCryptoTest, ShouldRefetchTrustedVaultKeysWhenChangeObserved) {
  const CoreAccountInfo kSyncingAccount =
      MakeAccountInfoWithGaia("syncingaccount");
  const std::vector<std::string> kInitialKeys = {"key1"};
  const std::vector<std::string> kNewKeys = {"key1", "key2"};

  trusted_vault_client_.StoreKeys(kSyncingAccount.gaia, kInitialKeys);

  // The engine replies with OnTrustedVaultKeyAccepted() only if |kNewKeys| are
  // provided.
  ON_CALL(engine_, AddTrustedVaultDecryptionKeys(_, _))
      .WillByDefault(
          [&](const std::vector<std::string>& keys, base::OnceClosure done_cb) {
            if (keys == kNewKeys) {
              crypto_.OnTrustedVaultKeyAccepted();
            }
            std::move(done_cb).Run();
          });

  // Mimic initialization of the engine where trusted vault keys are needed and
  // |kInitialKeys| are fetched, which are insufficient, and hence
  // IsTrustedVaultKeyRequired() is exposed.
  crypto_.SetSyncEngine(kSyncingAccount, &engine_);
  crypto_.OnTrustedVaultKeyRequired();
  ASSERT_THAT(trusted_vault_client_.fetch_count(), Eq(1));
  ASSERT_TRUE(trusted_vault_client_.CompleteFetchKeysRequest());
  ASSERT_TRUE(crypto_.IsTrustedVaultKeyRequired());

  // Mimic keys being added to the vault, which triggers a notification to
  // observers (namely |crypto_|), leading to a second fetch.
  trusted_vault_client_.StoreKeys(kSyncingAccount.gaia, kNewKeys);
  EXPECT_THAT(trusted_vault_client_.fetch_count(), Eq(2));
  EXPECT_CALL(reconfigure_cb_, Run(CONFIGURE_REASON_CRYPTO));
  EXPECT_TRUE(trusted_vault_client_.CompleteFetchKeysRequest());
  EXPECT_FALSE(crypto_.IsTrustedVaultKeyRequired());
}

// Same as above but the new keys become available during an ongoing FetchKeys()
// request.
TEST_F(SyncServiceCryptoTest,
       ShouldDeferTrustedVaultKeyFetchingWhenChangeObservedWhileOngoingFetch) {
  const CoreAccountInfo kSyncingAccount =
      MakeAccountInfoWithGaia("syncingaccount");
  const std::vector<std::string> kInitialKeys = {"key1"};
  const std::vector<std::string> kNewKeys = {"key1", "key2"};

  trusted_vault_client_.StoreKeys(kSyncingAccount.gaia, kInitialKeys);

  // The engine replies with OnTrustedVaultKeyAccepted() only if |kNewKeys| are
  // provided.
  ON_CALL(engine_, AddTrustedVaultDecryptionKeys(_, _))
      .WillByDefault(
          [&](const std::vector<std::string>& keys, base::OnceClosure done_cb) {
            if (keys == kNewKeys) {
              crypto_.OnTrustedVaultKeyAccepted();
            }
            std::move(done_cb).Run();
          });

  // Mimic initialization of the engine where trusted vault keys are needed and
  // |kInitialKeys| are in the process of being fetched.
  crypto_.SetSyncEngine(kSyncingAccount, &engine_);
  crypto_.OnTrustedVaultKeyRequired();
  ASSERT_THAT(trusted_vault_client_.fetch_count(), Eq(1));
  ASSERT_FALSE(crypto_.IsTrustedVaultKeyRequired());

  // While there is an ongoing fetch, mimic keys being added to the vault, which
  // triggers a notification to observers (namely |crypto_|).
  trusted_vault_client_.StoreKeys(kSyncingAccount.gaia, kNewKeys);

  // Because there's already an ongoing fetch, a second one should not have been
  // triggered yet and should be deferred instead.
  EXPECT_THAT(trusted_vault_client_.fetch_count(), Eq(1));

  // As soon as the first fetch completes, the second one (deferred) should be
  // started.
  EXPECT_TRUE(trusted_vault_client_.CompleteFetchKeysRequest());
  EXPECT_THAT(trusted_vault_client_.fetch_count(), Eq(2));
  EXPECT_FALSE(crypto_.IsTrustedVaultKeyRequired());

  // The completion of the second fetch should resolve the encryption issue.
  EXPECT_CALL(reconfigure_cb_, Run(CONFIGURE_REASON_CRYPTO));
  EXPECT_TRUE(trusted_vault_client_.CompleteFetchKeysRequest());
  EXPECT_THAT(trusted_vault_client_.fetch_count(), Eq(2));
  EXPECT_FALSE(crypto_.IsTrustedVaultKeyRequired());
}

// The engine gets initialized and the vault initially has insufficient keys,
// leading to IsTrustedVaultKeyRequired(). Later, keys are added to the vault
// *twice*, where the later event should be handled as a deferred fetch.
TEST_F(
    SyncServiceCryptoTest,
    ShouldDeferTrustedVaultKeyFetchingWhenChangeObservedWhileOngoingRefetch) {
  const CoreAccountInfo kSyncingAccount =
      MakeAccountInfoWithGaia("syncingaccount");
  const std::vector<std::string> kInitialKeys = {"key1"};
  const std::vector<std::string> kIntermediateKeys = {"key1", "key2"};
  const std::vector<std::string> kLatestKeys = {"key1", "key2", "key3"};

  trusted_vault_client_.StoreKeys(kSyncingAccount.gaia, kInitialKeys);

  // The engine replies with OnTrustedVaultKeyAccepted() only if |kLatestKeys|
  // are provided.
  ON_CALL(engine_, AddTrustedVaultDecryptionKeys(_, _))
      .WillByDefault(
          [&](const std::vector<std::string>& keys, base::OnceClosure done_cb) {
            if (keys == kLatestKeys) {
              crypto_.OnTrustedVaultKeyAccepted();
            }
            std::move(done_cb).Run();
          });

  // Mimic initialization of the engine where trusted vault keys are needed and
  // |kInitialKeys| are fetched, which are insufficient, and hence
  // IsTrustedVaultKeyRequired() is exposed.
  crypto_.SetSyncEngine(kSyncingAccount, &engine_);
  crypto_.OnTrustedVaultKeyRequired();
  ASSERT_THAT(trusted_vault_client_.fetch_count(), Eq(1));
  ASSERT_TRUE(trusted_vault_client_.CompleteFetchKeysRequest());
  ASSERT_TRUE(crypto_.IsTrustedVaultKeyRequired());

  // Mimic keys being added to the vault, which triggers a notification to
  // observers (namely |crypto_|), leading to a second fetch.
  trusted_vault_client_.StoreKeys(kSyncingAccount.gaia, kIntermediateKeys);
  EXPECT_THAT(trusted_vault_client_.fetch_count(), Eq(2));

  // While the second fetch is ongoing, mimic more keys being added to the
  // vault, which triggers a notification to observers (namely |crypto_|).
  trusted_vault_client_.StoreKeys(kSyncingAccount.gaia, kLatestKeys);

  // Because there's already an ongoing fetch, a third one should not have been
  // triggered yet and should be deferred instead.
  EXPECT_THAT(trusted_vault_client_.fetch_count(), Eq(2));

  // As soon as the second fetch completes, the third one (deferred) should be
  // started.
  EXPECT_TRUE(trusted_vault_client_.CompleteFetchKeysRequest());
  EXPECT_THAT(trusted_vault_client_.fetch_count(), Eq(3));
  EXPECT_TRUE(crypto_.IsTrustedVaultKeyRequired());

  // The completion of the third fetch should resolve the encryption issue.
  EXPECT_CALL(reconfigure_cb_, Run(CONFIGURE_REASON_CRYPTO));
  EXPECT_TRUE(trusted_vault_client_.CompleteFetchKeysRequest());
  EXPECT_THAT(trusted_vault_client_.fetch_count(), Eq(3));
  EXPECT_FALSE(crypto_.IsTrustedVaultKeyRequired());
}

}  // namespace

}  // namespace syncer
