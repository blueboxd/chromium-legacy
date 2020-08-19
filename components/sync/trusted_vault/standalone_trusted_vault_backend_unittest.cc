// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/sync/trusted_vault/standalone_trusted_vault_backend.h"

#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/test/bind_test_util.h"
#include "base/test/scoped_feature_list.h"
#include "components/os_crypt/os_crypt_mocker.h"
#include "components/sync/driver/sync_driver_switches.h"
#include "components/sync/trusted_vault/securebox.h"
#include "components/sync/trusted_vault/trusted_vault_connection.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace syncer {

namespace {

using testing::_;
using testing::Eq;
using testing::IsEmpty;
using testing::NotNull;

base::FilePath CreateUniqueTempDir(base::ScopedTempDir* temp_dir) {
  EXPECT_TRUE(temp_dir->CreateUniqueTempDir());
  return temp_dir->GetPath();
}

class MockTrustedVaultConnection : public TrustedVaultConnection {
 public:
  MockTrustedVaultConnection() = default;
  ~MockTrustedVaultConnection() override = default;

  MOCK_METHOD5(RegisterDevice,
               void(const CoreAccountInfo&,
                    const std::vector<uint8_t>&,
                    int,
                    const SecureBoxPublicKey&,
                    RegisterDeviceCallback));
  MOCK_METHOD5(DownloadKeys,
               void(const CoreAccountInfo&,
                    const std::vector<uint8_t>&,
                    int,
                    std::unique_ptr<SecureBoxKeyPair>,
                    DownloadKeysCallback));
};

class StandaloneTrustedVaultBackendTest : public testing::Test {
 public:
  StandaloneTrustedVaultBackendTest()
      : file_path_(
            CreateUniqueTempDir(&temp_dir_)
                .Append(base::FilePath(FILE_PATH_LITERAL("some_file")))) {
    override_features.InitAndEnableFeature(
        switches::kFollowTrustedVaultKeyRotation);
    auto connection =
        std::make_unique<testing::NiceMock<MockTrustedVaultConnection>>();
    connection_ = connection.get();
    backend_ = base::MakeRefCounted<StandaloneTrustedVaultBackend>(
        file_path_, std::move(connection));
  }

  ~StandaloneTrustedVaultBackendTest() override = default;

  void SetUp() override { OSCryptMocker::SetUp(); }

  void TearDown() override { OSCryptMocker::TearDown(); }

  MockTrustedVaultConnection* connection() { return connection_; }

  StandaloneTrustedVaultBackend* backend() { return backend_.get(); }

  // Stores |vault_keys| and mimics successful device registration, returns
  // private device key material.
  std::vector<uint8_t> StoreKeysAndMimicDeviceRegistration(
      const std::vector<std::vector<uint8_t>>& vault_keys,
      int last_vault_key_version,
      CoreAccountInfo account_info) {
    DCHECK(!vault_keys.empty());
    backend_->StoreKeys(account_info.gaia, vault_keys, last_vault_key_version);
    TrustedVaultConnection::RegisterDeviceCallback device_registration_callback;

    EXPECT_CALL(*connection_,
                RegisterDevice(Eq(account_info), Eq(vault_keys.back()),
                               Eq(last_vault_key_version), _, _))
        .WillOnce([&](const CoreAccountInfo&, const std::vector<uint8_t>&, int,
                      const SecureBoxPublicKey& device_public_key,
                      TrustedVaultConnection::RegisterDeviceCallback callback) {
          device_registration_callback = std::move(callback);
        });
    // Setting the syncing account will trigger device registration.
    backend()->SetSyncingAccount(account_info);
    EXPECT_FALSE(device_registration_callback.is_null());

    // Pretend that the registration completed successfully.
    std::move(device_registration_callback)
        .Run(TrustedVaultRequestStatus::kSuccess);

    // Reset syncing account.
    backend()->SetSyncingAccount(base::nullopt);

    std::string device_private_key_material =
        backend_->GetDeviceRegistrationInfoForTesting(account_info.gaia)
            .private_key_material();
    return std::vector<uint8_t>(device_private_key_material.begin(),
                                device_private_key_material.end());
  }

 private:
  base::test::ScopedFeatureList override_features;

  base::ScopedTempDir temp_dir_;
  const base::FilePath file_path_;
  testing::NiceMock<MockTrustedVaultConnection>* connection_;
  scoped_refptr<StandaloneTrustedVaultBackend> backend_;
};

TEST_F(StandaloneTrustedVaultBackendTest, ShouldRegisterDevice) {
  CoreAccountInfo account_info;
  account_info.gaia = "user";

  const std::vector<uint8_t> kVaultKey = {1, 2, 3};
  const int kLastKeyVersion = 0;

  backend()->StoreKeys(account_info.gaia, {kVaultKey}, kLastKeyVersion);

  TrustedVaultConnection::RegisterDeviceCallback device_registration_callback;
  std::vector<uint8_t> serialized_public_device_key;
  EXPECT_CALL(*connection(), RegisterDevice(Eq(account_info), Eq(kVaultKey),
                                            Eq(kLastKeyVersion), _, _))
      .WillOnce([&](const CoreAccountInfo&, const std::vector<uint8_t>&, int,
                    const SecureBoxPublicKey& device_public_key,
                    TrustedVaultConnection::RegisterDeviceCallback callback) {
        serialized_public_device_key = device_public_key.ExportToBytes();
        device_registration_callback = std::move(callback);
      });

  // Setting the syncing account will trigger device registration.
  backend()->SetSyncingAccount(account_info);
  ASSERT_FALSE(device_registration_callback.is_null());

  // Pretend that the registration completed successfully.
  std::move(device_registration_callback)
      .Run(TrustedVaultRequestStatus::kSuccess);

  // Now the device should be registered.
  sync_pb::LocalDeviceRegistrationInfo registration_info =
      backend()->GetDeviceRegistrationInfoForTesting(account_info.gaia);
  EXPECT_TRUE(registration_info.device_registered());
  EXPECT_TRUE(registration_info.has_private_key_material());

  std::unique_ptr<SecureBoxKeyPair> key_pair =
      SecureBoxKeyPair::CreateByPrivateKeyImport(base::as_bytes(
          base::make_span(registration_info.private_key_material())));
  EXPECT_THAT(key_pair->public_key().ExportToBytes(),
              Eq(serialized_public_device_key));
}

// Unless keys marked as stale, FetchKeys() should be completed immediately,
// without keys download attempt.
TEST_F(StandaloneTrustedVaultBackendTest, ShouldFetchKeysImmediately) {
  CoreAccountInfo account_info;
  account_info.gaia = "user";

  const std::vector<std::vector<uint8_t>> kVaultKeys = {{1, 2, 3}};
  const int kLastKeyVersion = 0;

  // Make keys downloading theoretically possible.
  StoreKeysAndMimicDeviceRegistration(kVaultKeys, kLastKeyVersion,
                                      account_info);
  backend()->SetSyncingAccount(account_info);

  EXPECT_CALL(*connection(), DownloadKeys(_, _, _, _, _)).Times(0);

  std::vector<std::vector<uint8_t>> fetched_keys;
  // Callback should be called immediately.
  backend()->FetchKeys(account_info,
                       base::BindLambdaForTesting(
                           [&](const std::vector<std::vector<uint8_t>>& keys) {
                             fetched_keys = keys;
                           }));

  EXPECT_THAT(fetched_keys, Eq(kVaultKeys));
}

TEST_F(StandaloneTrustedVaultBackendTest, ShouldDownloadKeys) {
  CoreAccountInfo account_info;
  account_info.gaia = "user";

  const std::vector<uint8_t> kInitialVaultKey = {1, 2, 3};
  const int kInitialLastKeyVersion = 0;

  std::vector<uint8_t> private_device_key_material =
      StoreKeysAndMimicDeviceRegistration({kInitialVaultKey},
                                          kInitialLastKeyVersion, account_info);
  EXPECT_TRUE(backend()->MarkKeysAsStale(account_info));
  backend()->SetSyncingAccount(account_info);

  const std::vector<std::vector<uint8_t>> kNewVaultKeys = {kInitialVaultKey,
                                                           {1, 3, 2}};
  const int kNewLastKeyVersion = 1;

  std::unique_ptr<SecureBoxKeyPair> device_key_pair;
  TrustedVaultConnection::DownloadKeysCallback download_keys_callback;
  EXPECT_CALL(*connection(),
              DownloadKeys(Eq(account_info), Eq(kInitialVaultKey),
                           Eq(kInitialLastKeyVersion), _, _))
      .WillOnce([&](const CoreAccountInfo&, const std::vector<uint8_t>&, int,
                    std::unique_ptr<SecureBoxKeyPair> key_pair,
                    TrustedVaultConnection::DownloadKeysCallback callback) {
        device_key_pair = std::move(key_pair);
        download_keys_callback = std::move(callback);
      });

  std::vector<std::vector<uint8_t>> fetched_keys;
  // FetchKeys() should trigger keys downloading.
  backend()->FetchKeys(account_info,
                       base::BindLambdaForTesting(
                           [&](const std::vector<std::vector<uint8_t>>& keys) {
                             fetched_keys = keys;
                           }));
  ASSERT_FALSE(download_keys_callback.is_null());
  ASSERT_THAT(fetched_keys, IsEmpty());

  // Ensure that the right device key was passed into DonwloadKeys().
  ASSERT_THAT(device_key_pair, NotNull());
  EXPECT_THAT(device_key_pair->private_key().ExportToBytes(),
              Eq(private_device_key_material));

  // Mimic successful key downloading.
  std::move(download_keys_callback)
      .Run(TrustedVaultRequestStatus::kSuccess, kNewVaultKeys,
           kNewLastKeyVersion);

  // Now fetch keys attempt should be completed.
  EXPECT_THAT(fetched_keys, Eq(kNewVaultKeys));
}

}  // namespace

}  // namespace syncer
