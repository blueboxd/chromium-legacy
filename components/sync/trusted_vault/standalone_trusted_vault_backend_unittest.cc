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

  const std::vector<uint8_t> kVaultKey = {{1, 2, 3}};
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

}  // namespace

}  // namespace syncer
