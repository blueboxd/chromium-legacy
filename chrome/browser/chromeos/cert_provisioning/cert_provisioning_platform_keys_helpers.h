// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_CERT_PROVISIONING_CERT_PROVISIONING_PLATFORM_KEYS_HELPERS_H_
#define CHROME_BROWSER_CHROMEOS_CERT_PROVISIONING_CERT_PROVISIONING_PLATFORM_KEYS_HELPERS_H_

#include "base/callback.h"
#include "base/memory/weak_ptr.h"
#include "chrome/browser/chromeos/cert_provisioning/cert_provisioning_common.h"
#include "net/cert/x509_certificate.h"

namespace chromeos {
namespace platform_keys {
class PlatformKeysService;
}  // namespace platform_keys
}  // namespace chromeos

namespace chromeos {
namespace cert_provisioning {

// ========= CertProvisioningCertsWithIdsGetter ================================

using GetCertsWithIdsCallback = base::OnceCallback<void(
    std::map<std::string, scoped_refptr<net::X509Certificate>> certs_with_ids,
    const std::string& error_message)>;

// Helper class that retrieves list of all certificates in a given scope with
// their certificate profile ids. Certificates without the id are ignored.
class CertProvisioningCertsWithIdsGetter {
 public:
  CertProvisioningCertsWithIdsGetter();
  CertProvisioningCertsWithIdsGetter(
      const CertProvisioningCertsWithIdsGetter&) = delete;
  CertProvisioningCertsWithIdsGetter& operator=(
      const CertProvisioningCertsWithIdsGetter&) = delete;
  ~CertProvisioningCertsWithIdsGetter();

  bool IsRunning() const;

  void GetCertsWithIds(
      CertScope cert_scope,
      platform_keys::PlatformKeysService* platform_keys_service,
      GetCertsWithIdsCallback callback);

 private:
  void OnGetCertificatesDone(
      std::unique_ptr<net::CertificateList> existing_certs,
      const std::string& error_message);

  void CollectOneResult(scoped_refptr<net::X509Certificate> cert,
                        const std::string& cert_id,
                        const std::string& error_message);

  CertScope cert_scope_ = CertScope::kDevice;
  platform_keys::PlatformKeysService* platform_keys_service_ = nullptr;

  size_t wait_counter_ = 0;
  std::map<std::string, scoped_refptr<net::X509Certificate>> certs_with_ids_;
  GetCertsWithIdsCallback callback_;

  SEQUENCE_CHECKER(sequence_checker_);
  base::WeakPtrFactory<CertProvisioningCertsWithIdsGetter> weak_factory_{this};
};

// ========= CertProvisioningCertDeleter =======================================

using DeleteCertsCallback =
    base::OnceCallback<void(const std::string& error_message)>;

// Helper class that deletes all certificates in a given scope with certificate
// profile ids that are not specified to be kept. Certificates without the id
// are ignored.
class CertProvisioningCertDeleter {
 public:
  CertProvisioningCertDeleter();
  CertProvisioningCertDeleter(const CertProvisioningCertDeleter&) = delete;
  CertProvisioningCertDeleter& operator=(const CertProvisioningCertDeleter&) =
      delete;
  ~CertProvisioningCertDeleter();

  void DeleteCerts(CertScope cert_scope,
                   platform_keys::PlatformKeysService* platform_keys_service,
                   const std::set<std::string>& cert_profile_ids_to_keep,
                   DeleteCertsCallback callback);

 private:
  void OnGetCertsWithIdsDone(
      std::map<std::string, scoped_refptr<net::X509Certificate>> certs_with_ids,
      const std::string& error_message);

  void OnRemoveCertificateDone(const std::string& error_message);

  // Keeps track of how many certificates are already processed. Calls the
  // |callback_| when all work is done.
  void AccountOneResult();

  CertScope cert_scope_ = CertScope::kDevice;
  platform_keys::PlatformKeysService* platform_keys_service_ = nullptr;

  size_t wait_counter_ = 0;
  std::set<std::string> cert_profile_ids_to_keep_;
  DeleteCertsCallback callback_;

  std::unique_ptr<CertProvisioningCertsWithIdsGetter> cert_getter_;

  SEQUENCE_CHECKER(sequence_checker_);
  base::WeakPtrFactory<CertProvisioningCertDeleter> weak_factory_{this};
};

}  // namespace cert_provisioning
}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_CERT_PROVISIONING_CERT_PROVISIONING_PLATFORM_KEYS_HELPERS_H_
