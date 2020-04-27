// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_PLATFORM_KEYS_MOCK_PLATFORM_KEYS_SERVICE_H_
#define CHROME_BROWSER_CHROMEOS_PLATFORM_KEYS_MOCK_PLATFORM_KEYS_SERVICE_H_

#include <memory>

#include "base/callback.h"
#include "chrome/browser/chromeos/platform_keys/platform_keys_service.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace chromeos {
namespace platform_keys {

class MockPlatformKeysService : public PlatformKeysService {
 public:
  MockPlatformKeysService();
  MockPlatformKeysService(const MockPlatformKeysService&) = delete;
  MockPlatformKeysService& operator=(const MockPlatformKeysService&) = delete;
  ~MockPlatformKeysService() override;

  MOCK_METHOD(void,
              GenerateRSAKey,
              (const std::string& token_id,
               unsigned int modulus_length_bits,
               const GenerateKeyCallback& callback),
              (override));

  MOCK_METHOD(void,
              GenerateECKey,
              (const std::string& token_id,
               const std::string& named_curve,
               const GenerateKeyCallback& callback),
              (override));

  MOCK_METHOD(void,
              SignRSAPKCS1Digest,
              (const std::string& token_id,
               const std::string& data,
               const std::string& public_key_spki_der,
               HashAlgorithm hash_algorithm,
               const SignCallback& callback),
              (override));

  MOCK_METHOD(void,
              SignRSAPKCS1Raw,
              (const std::string& token_id,
               const std::string& data,
               const std::string& public_key_spki_der,
               const SignCallback& callback),
              (override));

  MOCK_METHOD(void,
              SignECDSADigest,
              (const std::string& token_id,
               const std::string& data,
               const std::string& public_key_spki_der,
               HashAlgorithm hash_algorithm,
               const SignCallback& callback),
              (override));

  MOCK_METHOD(void,
              SelectClientCertificates,
              (const std::vector<std::string>& certificate_authorities,
               const SelectCertificatesCallback& callback),
              (override));

  MOCK_METHOD(void,
              GetCertificates,
              (const std::string& token_id,
               const GetCertificatesCallback& callback),
              (override));

  MOCK_METHOD(void,
              GetAllKeys,
              (const std::string& token_id, GetAllKeysCallback callback),
              (override));

  MOCK_METHOD(void,
              ImportCertificate,
              (const std::string& token_id,
               const scoped_refptr<net::X509Certificate>& certificate,
               const ImportCertificateCallback& callback),
              (override));

  MOCK_METHOD(void,
              RemoveCertificate,
              (const std::string& token_id,
               const scoped_refptr<net::X509Certificate>& certificate,
               const RemoveCertificateCallback& callback),
              (override));

  MOCK_METHOD(void,
              RemoveKey,
              (const std::string& token_id,
               const std::string& public_key_spki_der,
               RemoveKeyCallback callback),
              (override));

  MOCK_METHOD(void, GetTokens, (const GetTokensCallback& callback), (override));

  MOCK_METHOD(void,
              GetKeyLocations,
              (const std::string& public_key_spki_der,
               const GetKeyLocationsCallback& callback),
              (override));

  MOCK_METHOD(void,
              SetAttributeForKey,
              (const std::string& token_id,
               const std::string& public_key_spki_der,
               KeyAttributeType attribute_type,
               const std::string& attribute_value,
               SetAttributeForKeyCallback callback),
              (override));

  MOCK_METHOD(void,
              GetAttributeForKey,
              (const std::string& token_id,
               const std::string& public_key_spki_der,
               KeyAttributeType attribute_type,
               GetAttributeForKeyCallback callback),
              (override));

  MOCK_METHOD(void,
              SetMapToSoftokenAttrsForTesting,
              (bool map_to_softoken_attrs_for_testing),
              (override));
};

std::unique_ptr<KeyedService> BuildMockPlatformKeysService(
    content::BrowserContext* context);

}  // namespace platform_keys
}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_PLATFORM_KEYS_MOCK_PLATFORM_KEYS_SERVICE_H_
