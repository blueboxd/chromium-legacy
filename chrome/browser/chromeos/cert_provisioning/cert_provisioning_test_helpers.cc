// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/cert_provisioning/cert_provisioning_test_helpers.h"

#include "chrome/browser/chromeos/profiles/profile_helper.h"
#include "chrome/test/base/testing_browser_process.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace chromeos {
namespace cert_provisioning {

//================ ProfileHelperForTesting =====================================

namespace {
const char kTestUserEmail[] = "user@gmail.com";
const char kTestUserGaiaId[] = "test_gaia_id";
}  // namespace

ProfileHelperForTesting::ProfileHelperForTesting()
    : testing_profile_manager_(TestingBrowserProcess::GetGlobal()) {
  Init();
}

ProfileHelperForTesting::~ProfileHelperForTesting() = default;

void ProfileHelperForTesting::Init() {
  ASSERT_TRUE(testing_profile_manager_.SetUp());

  testing_profile_ =
      testing_profile_manager_.CreateTestingProfile(kTestUserEmail);
  ASSERT_TRUE(testing_profile_);

  auto test_account =
      AccountId::FromUserEmailGaiaId(kTestUserEmail, kTestUserGaiaId);
  fake_user_manager_.AddUser(test_account);

  ProfileHelper::Get()->SetUserToProfileMappingForTesting(
      fake_user_manager_.GetPrimaryUser(), testing_profile_);
}

Profile* ProfileHelperForTesting::GetProfile() const {
  return testing_profile_;
}

//================ SpyingFakeCryptohomeClient ==================================

SpyingFakeCryptohomeClient::SpyingFakeCryptohomeClient() = default;
SpyingFakeCryptohomeClient::~SpyingFakeCryptohomeClient() = default;

void SpyingFakeCryptohomeClient::TpmAttestationDeleteKey(
    attestation::AttestationKeyType key_type,
    const cryptohome::AccountIdentifier& cryptohome_id,
    const std::string& key_prefix,
    DBusMethodCallback<bool> callback) {
  OnTpmAttestationDeleteKey(key_type, key_prefix);
  FakeCryptohomeClient::TpmAttestationDeleteKey(
      key_type, cryptohome_id, key_prefix, std::move(callback));
}

void SpyingFakeCryptohomeClient::TpmAttestationDeleteKeysByPrefix(
    attestation::AttestationKeyType key_type,
    const cryptohome::AccountIdentifier& cryptohome_id,
    const std::string& key_prefix,
    DBusMethodCallback<bool> callback) {
  OnTpmAttestationDeleteKeysByPrefix(key_type, key_prefix);
  FakeCryptohomeClient::TpmAttestationDeleteKeysByPrefix(
      key_type, cryptohome_id, key_prefix, std::move(callback));
}

}  // namespace cert_provisioning
}  // namespace chromeos
