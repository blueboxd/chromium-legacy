// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_CERT_PROVISIONING_CERT_PROVISIONING_TEST_HELPERS_H_
#define CHROME_BROWSER_CHROMEOS_CERT_PROVISIONING_CERT_PROVISIONING_TEST_HELPERS_H_

#include "chrome/browser/chromeos/login/users/fake_chrome_user_manager.h"
#include "chrome/test/base/testing_profile_manager.h"
#include "chromeos/dbus/cryptohome/fake_cryptohome_client.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace chromeos {
namespace cert_provisioning {

//================ ProfileHelperForTesting =====================================

class ProfileHelperForTesting {
 public:
  ProfileHelperForTesting();
  ProfileHelperForTesting(const ProfileHelperForTesting&) = delete;
  ProfileHelperForTesting& operator=(const ProfileHelperForTesting&) = delete;
  ~ProfileHelperForTesting();

  Profile* GetProfile() const;

 private:
  void Init();

  TestingProfileManager testing_profile_manager_;
  FakeChromeUserManager fake_user_manager_;
  TestingProfile* testing_profile_ = nullptr;
};

//================ SpyingFakeCryptohomeClient ==================================

class SpyingFakeCryptohomeClient : public FakeCryptohomeClient {
 public:
  SpyingFakeCryptohomeClient();
  SpyingFakeCryptohomeClient(const SpyingFakeCryptohomeClient&) = delete;
  SpyingFakeCryptohomeClient& operator=(const SpyingFakeCryptohomeClient&) =
      delete;
  ~SpyingFakeCryptohomeClient() override;

  void TpmAttestationDeleteKey(
      attestation::AttestationKeyType key_type,
      const cryptohome::AccountIdentifier& cryptohome_id,
      const std::string& key_prefix,
      DBusMethodCallback<bool> callback) override;

  void TpmAttestationDeleteKeysByPrefix(
      attestation::AttestationKeyType key_type,
      const cryptohome::AccountIdentifier& cryptohome_id,
      const std::string& key_prefix,
      DBusMethodCallback<bool> callback) override;

  MOCK_METHOD(void,
              OnTpmAttestationDeleteKey,
              (attestation::AttestationKeyType key_type,
               const std::string& key_prefix));

  MOCK_METHOD(void,
              OnTpmAttestationDeleteKeysByPrefix,
              (attestation::AttestationKeyType key_type,
               const std::string& key_prefix));
};

}  // namespace cert_provisioning
}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_CERT_PROVISIONING_CERT_PROVISIONING_TEST_HELPERS_H_
