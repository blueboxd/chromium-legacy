// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_PLATFORM_KEYS_PLATFORM_KEYS_SERVICE_H_
#define CHROME_BROWSER_CHROMEOS_PLATFORM_KEYS_PLATFORM_KEYS_SERVICE_H_

#include <stddef.h>

#include <memory>
#include <string>
#include <vector>

#include "base/callback_forward.h"
#include "base/memory/ref_counted.h"
#include "base/memory/weak_ptr.h"
#include "components/keyed_service/core/keyed_service.h"
#include "net/cert/x509_certificate.h"

namespace content {
class BrowserContext;
}

namespace chromeos {
namespace platform_keys {

// A token is a store for keys or certs and can provide cryptographic
// operations.
// ChromeOS provides itself a user token and conditionally a system wide token,
// thus these tokens use static identifiers. The platform keys API is designed
// to support arbitrary other tokens in the future, which could then use
// run-time generated IDs.
extern const char kTokenIdUser[];
extern const char kTokenIdSystem[];

// Supported key types.
enum class KeyType { kRsassaPkcs1V15, kEcdsa };

// Supported key attribute types.
enum class KeyAttributeType { CertificateProvisioningId };

// Supported hash algorithms.
enum HashAlgorithm {
  HASH_ALGORITHM_NONE,  // The value if no hash function is selected.
  HASH_ALGORITHM_SHA1,
  HASH_ALGORITHM_SHA256,
  HASH_ALGORITHM_SHA384,
  HASH_ALGORITHM_SHA512
};

// Returns the DER encoding of the X.509 Subject Public Key Info of the public
// key in |certificate|.
std::string GetSubjectPublicKeyInfo(
    const scoped_refptr<net::X509Certificate>& certificate);

// Intersects the two certificate lists |certs1| and |certs2| and passes the
// intersection to |callback|. The intersction preserves the order of |certs1|.
void IntersectCertificates(
    const net::CertificateList& certs1,
    const net::CertificateList& certs2,
    const base::Callback<void(std::unique_ptr<net::CertificateList>)>&
        callback);

// Obtains information about the public key in |certificate|.
// If |certificate| contains an RSA key, sets |key_size_bits| to the modulus
// length, and |key_type| to type RSA and returns true.
// If |certificate| contains any other key type, or if the public exponent of
// the RSA key in |certificate| is not F4, returns false and does not update any
// of the output parameters.
// All pointer arguments must not be null.
bool GetPublicKey(const scoped_refptr<net::X509Certificate>& certificate,
                  net::X509Certificate::PublicKeyType* key_type,
                  size_t* key_size_bits);

struct ClientCertificateRequest {
  ClientCertificateRequest();
  ClientCertificateRequest(const ClientCertificateRequest& other);
  ~ClientCertificateRequest();

  // The list of the types of certificates requested, sorted in order of the
  // server's preference.
  std::vector<net::X509Certificate::PublicKeyType> certificate_key_types;

  // List of distinguished names of certificate authorities allowed by the
  // server. Each entry must be a DER-encoded X.509 DistinguishedName.
  std::vector<std::string> certificate_authorities;
};

using GenerateKeyCallback =
    base::Callback<void(const std::string& public_key_spki_der,
                        const std::string& error_message)>;

using SignCallback = base::Callback<void(const std::string& signature,
                                         const std::string& error_message)>;

// If the certificate request could be processed successfully, |matches| will
// contain the list of matching certificates (which may be empty) and
// |error_message| will be empty. If an error occurred, |matches| will be null
// and |error_message| contain an error message.
using SelectCertificatesCallback =
    base::Callback<void(std::unique_ptr<net::CertificateList> matches,
                        const std::string& error_message)>;

// If the list of certificates could be successfully retrieved, |certs| will
// contain the list of available certificates (maybe empty) and |error_message|
// will be empty. If an error occurred, |certs| will be empty and
// |error_message| contain an error message.
using GetCertificatesCallback =
    base::Callback<void(std::unique_ptr<net::CertificateList> certs,
                        const std::string& error_message)>;

// If the list of key pairs could be successfully retrieved, |error_message|
// will be empty and |public_key_spki_der_list| will contain the list of
// available key pairs (may be empty if no key pairs exist). Each available key
// pair is represented as a DER-encoded SubjectPublicKeyInfo. If an error
// occurred, |public_key_spki_der_list| will be empty and |error_message|
// will be set to an error message.
using GetAllKeysCallback =
    base::OnceCallback<void(std::vector<std::string> public_key_spki_der_list,
                            const std::string& error_message)>;

// If an error occurred during import, |error_message| will be set to an error
// message.
using ImportCertificateCallback =
    base::Callback<void(const std::string& error_message)>;

// If an error occurred during removal, |error_message| will be set to an error
// message.
using RemoveCertificateCallback =
    base::Callback<void(const std::string& error_message)>;

// If the the key pair has been successfully removed, |error_message| will be
// empty. If an error occurs, |error_message| will be set to the error message.
using RemoveKeyCallback =
    base::OnceCallback<void(const std::string& error_message)>;

// If the list of available tokens could be successfully retrieved, |token_ids|
// will contain the token ids. If an error occurs, |token_ids| will be nullptr
// and |error_message| will be set to an error message.
using GetTokensCallback =
    base::Callback<void(std::unique_ptr<std::vector<std::string>> token_ids,
                        const std::string& error_message)>;

// If token ids have been successfully retrieved, |error_message| will be empty.
// Two cases are possible then:
// If |token_ids| is not empty, |token_ids| has been filled with the identifiers
// of the tokens the private key was found on and the user has access to.
// Currently, valid token identifiers are |kTokenIdUser| and |kTokenIdSystem|.
// If |token_ids| is empty, the private key has not been found on any token the
// user has access to. Note that this is also the case if the key exists on the
// system token, but the current user does not have access to the system token.
// If an error occurred during processing, |token_ids| will be empty and
// |error_message| will be set to an error message.
// TODO(pmarko): This is currently a RepeatingCallback because of
// GetNSSCertDatabaseForResourceContext semantics.
using GetKeyLocationsCallback =
    base::RepeatingCallback<void(const std::vector<std::string>& token_ids,
                                 const std::string& error_message)>;

// If the attribute value has been successfully set, |error_message| will be
// empty.
// If an error occurs, |error_message| will be set to the error message.
using SetAttributeForKeyCallback =
    base::OnceCallback<void(const std::string& error_message)>;

// If the attribute value has been successfully retrieved, |attribute_value|
// will contain the result and |error_message| will be empty.
// If an error occurs, |attribute_value| will be empty and |error_message| will
// be set to the error message.
using GetAttributeForKeyCallback =
    base::OnceCallback<void(const std::string& attribute_value,
                            const std::string& error_message)>;

// Functions of this class shouldn't be called directly from the context of
// an extension. Instead use ExtensionPlatformKeysService which enforces
// restrictions upon extensions.
// All public methods of this class should be called on the UI thread.
class PlatformKeysService : public KeyedService {
 public:
  PlatformKeysService() = default;
  PlatformKeysService(const PlatformKeysService&) = delete;
  PlatformKeysService& operator=(const PlatformKeysService&) = delete;
  ~PlatformKeysService() override = default;

  // Generates a RSA key pair with |modulus_length_bits|. |token_id| specifies
  // the token to store the key pair on and can currently be |kTokenIdUser| or
  // |kTokenIdSystem|. |callback| will be invoked with the resulting public key
  // or an error.
  virtual void GenerateRSAKey(const std::string& token_id,
                              unsigned int modulus_length_bits,
                              const GenerateKeyCallback& callback) = 0;

  // Generates a EC key pair with |named_curve|. |token_id| specifies the token
  // to store the key pair on and can currently be |kTokenIdUser| or
  // |kTokenIdSystem|. |callback| will be invoked with the resulting public key
  // or an error.
  virtual void GenerateECKey(const std::string& token_id,
                             const std::string& named_curve,
                             const GenerateKeyCallback& callback) = 0;

  // Digests |data|, applies PKCS1 padding and afterwards signs the data with
  // the private key matching |public_key_spki_der|. If a non empty token id is
  // provided and the key is not found in that token, the operation aborts.
  // |callback| will be invoked with the signature or an error message.
  virtual void SignRSAPKCS1Digest(const std::string& token_id,
                                  const std::string& data,
                                  const std::string& public_key_spki_der,
                                  HashAlgorithm hash_algorithm,
                                  const SignCallback& callback) = 0;

  // Applies PKCS1 padding and afterwards signs the data with the private key
  // matching |public_key_spki_der|. |data| is not digested. If a non empty
  // token id is provided and the key is not found in that token, the operation
  // aborts. The size of |data| (number of octets) must be smaller than k - 11,
  // where k is the key size in octets. |callback| will be invoked with the
  // signature or an error message.
  virtual void SignRSAPKCS1Raw(const std::string& token_id,
                               const std::string& data,
                               const std::string& public_key_spki_der,
                               const SignCallback& callback) = 0;

  // Digests |data| and afterwards signs the data with the private key matching
  // |public_key_spki_der|. If a non empty token id is provided and the key is
  // not found in that token, the operation aborts. |callback| will be invoked
  // with the ECDSA signature or an error message.
  virtual void SignECDSADigest(const std::string& token_id,
                               const std::string& data,
                               const std::string& public_key_spki_der,
                               HashAlgorithm hash_algorithm,
                               const SignCallback& callback) = 0;

  // Returns the list of all certificates that were issued by one of the
  // |certificate_authorities|. If |certificate_authorities| is empty, all
  // certificates will be returned. |callback| will be invoked with the matches
  // or an error message.
  virtual void SelectClientCertificates(
      const std::vector<std::string>& certificate_authorities,
      const SelectCertificatesCallback& callback) = 0;

  // Returns the list of all certificates with stored private key available from
  // the given token. If an empty |token_id| is provided, all certificates the
  // user associated with |browser_context| has access to are listed. Otherwise,
  // only certificates from the specified token are listed. |callback| will be
  // invoked with the list of available certificates or an error message.
  virtual void GetCertificates(const std::string& token_id,
                               const GetCertificatesCallback& callback) = 0;

  // Returns the list of all keys available from the given |token_id| as a list
  // of der-encoded SubjectPublicKeyInfo strings. |callback| will be invoked on
  // the UI thread with the list of available public keys, possibly with an
  // error message.
  virtual void GetAllKeys(const std::string& token_id,
                          GetAllKeysCallback callback) = 0;

  // Imports |certificate| to the given token if the certified key is already
  // stored in this token. Any intermediate of |certificate| will be ignored.
  // |token_id| specifies the token to store the certificate on and can
  // currently be |kTokenIdUser| or |kTokenIdSystem|. The private key must be
  // stored on the same token. |callback| will be invoked when the import is
  // finished, possibly with an error message.
  virtual void ImportCertificate(
      const std::string& token_id,
      const scoped_refptr<net::X509Certificate>& certificate,
      const ImportCertificateCallback& callback) = 0;

  // Removes |certificate| from the given token if present. Any intermediate of
  // |certificate| will be ignored. |token_id| specifies the token to remove the
  // certificate from and can currently be empty (any token), |kTokenIdUser| or
  // |kTokenIdSystem|. |callback| will be invoked when the removal is finished,
  // possibly with an error message.
  virtual void RemoveCertificate(
      const std::string& token_id,
      const scoped_refptr<net::X509Certificate>& certificate,
      const RemoveCertificateCallback& callback) = 0;

  // Removes the key pair if no matching certificates exist. Only keys in the
  // given |token_id| are considered. |callback| will be invoked on the UI
  // thread when the removal is finished, possibly with an error message.
  virtual void RemoveKey(const std::string& token_id,
                         const std::string& public_key_spki_der,
                         RemoveKeyCallback callback) = 0;

  // Gets the list of available tokens. |callback| will be invoked when the list
  // of available tokens is determined, possibly with an error message.
  // Calls |callback| on the UI thread.
  virtual void GetTokens(const GetTokensCallback& callback) = 0;

  // Determines the token(s) on which the private key corresponding to
  // |public_key_spki_der| is stored. |callback| will be invoked when the token
  // ids are determined, possibly with an error message. Calls |callback| on the
  // UI thread.
  virtual void GetKeyLocations(const std::string& public_key_spki_der,
                               const GetKeyLocationsCallback& callback) = 0;

  // Sets |attribute_type| for the private key corresponding to
  // |public_key_spki_der| to |attribute_value| only if the key is in
  // |token_id|. |callback| will be invoked on the UI thread when setting the
  // attribute is done, possibly with an error message.
  virtual void SetAttributeForKey(const std::string& token_id,
                                  const std::string& public_key_spki_der,
                                  KeyAttributeType attribute_type,
                                  const std::string& attribute_value,
                                  SetAttributeForKeyCallback callback) = 0;

  // Gets |attribute_type| for the private key corresponding to
  // |public_key_spki_der| only if the key is in |token_id|.
  // |callback| will be invoked on the UI thread when getting the attribute
  // is done, possibly with an error message.
  virtual void GetAttributeForKey(const std::string& token_id,
                                  const std::string& public_key_spki_der,
                                  KeyAttributeType attribute_type,
                                  GetAttributeForKeyCallback callback) = 0;

  // Softoken NSS PKCS11 module (used for testing) allows only predefined key
  // attributes to be set and retrieved. Chaps supports setting and retrieving
  // custom attributes.
  // If |map_to_softoken_attrs_for_testing| is true, the service will use
  // fake KeyAttribute mappings predefined in softoken module for testing.
  // Otherwise, the real mappings to constants in
  // third_party/cros_system_api/constants/pkcs11_custom_attributes.h will be
  // used.
  virtual void SetMapToSoftokenAttrsForTesting(
      const bool map_to_softoken_attrs_for_testing) = 0;
};

class PlatformKeysServiceImpl final : public PlatformKeysService {
 public:
  explicit PlatformKeysServiceImpl(content::BrowserContext* context);
  ~PlatformKeysServiceImpl() override;

  // PlatformKeysService
  void GenerateRSAKey(const std::string& token_id,
                      unsigned int modulus_length_bits,
                      const GenerateKeyCallback& callback) override;
  void GenerateECKey(const std::string& token_id,
                     const std::string& named_curve,
                     const GenerateKeyCallback& callback) override;
  void SignRSAPKCS1Digest(const std::string& token_id,
                          const std::string& data,
                          const std::string& public_key_spki_der,
                          HashAlgorithm hash_algorithm,
                          const SignCallback& callback) override;
  void SignRSAPKCS1Raw(const std::string& token_id,
                       const std::string& data,
                       const std::string& public_key_spki_der,
                       const SignCallback& callback) override;
  void SignECDSADigest(const std::string& token_id,
                       const std::string& data,
                       const std::string& public_key_spki_der,
                       HashAlgorithm hash_algorithm,
                       const SignCallback& callback) override;
  void SelectClientCertificates(
      const std::vector<std::string>& certificate_authorities,
      const SelectCertificatesCallback& callback) override;
  void GetCertificates(const std::string& token_id,
                       const GetCertificatesCallback& callback) override;
  void GetAllKeys(const std::string& token_id,
                  GetAllKeysCallback callback) override;
  void ImportCertificate(const std::string& token_id,
                         const scoped_refptr<net::X509Certificate>& certificate,
                         const ImportCertificateCallback& callback) override;
  void RemoveCertificate(const std::string& token_id,
                         const scoped_refptr<net::X509Certificate>& certificate,
                         const RemoveCertificateCallback& callback) override;
  void RemoveKey(const std::string& token_id,
                 const std::string& public_key_spki_der,
                 RemoveKeyCallback callback) override;
  void GetTokens(const GetTokensCallback& callback) override;
  void GetKeyLocations(const std::string& public_key_spki_der,
                       const GetKeyLocationsCallback& callback) override;
  void SetAttributeForKey(const std::string& token_id,
                          const std::string& public_key_spki_der,
                          KeyAttributeType attribute_type,
                          const std::string& attribute_value,
                          SetAttributeForKeyCallback callback) override;
  void GetAttributeForKey(const std::string& token_id,
                          const std::string& public_key_spki_der,
                          KeyAttributeType attribute_type,
                          GetAttributeForKeyCallback callback) override;
  void SetMapToSoftokenAttrsForTesting(
      bool map_to_softoken_attrs_for_testing) override;
  bool IsSetMapToSoftokenAttrsForTesting();

 private:
  content::BrowserContext* const browser_context_;
  bool map_to_softoken_attrs_for_testing_ = false;
  base::WeakPtrFactory<PlatformKeysServiceImpl> weak_factory_{this};
};

}  // namespace platform_keys
}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_PLATFORM_KEYS_PLATFORM_KEYS_SERVICE_H_
