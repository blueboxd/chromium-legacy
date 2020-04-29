// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/cert_provisioning/cert_provisioning_platform_keys_helpers.h"
#include <memory>

#include "base/bind.h"
#include "base/check.h"
#include "base/stl_util.h"
#include "chrome/browser/chromeos/cert_provisioning/cert_provisioning_common.h"
#include "chrome/browser/chromeos/platform_keys/platform_keys_service.h"

namespace chromeos {
namespace cert_provisioning {

// ========= CertProvisioningCertsWithIdsGetter ================================

CertProvisioningCertsWithIdsGetter::CertProvisioningCertsWithIdsGetter() =
    default;

CertProvisioningCertsWithIdsGetter::~CertProvisioningCertsWithIdsGetter() =
    default;

bool CertProvisioningCertsWithIdsGetter::IsRunning() const {
  return !callback_.is_null();
}

void CertProvisioningCertsWithIdsGetter::GetCertsWithIds(
    CertScope cert_scope,
    platform_keys::PlatformKeysService* platform_keys_service,
    GetCertsWithIdsCallback callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(platform_keys_service);
  DCHECK(callback_.is_null());

  cert_scope_ = cert_scope;
  platform_keys_service_ = platform_keys_service;
  callback_ = std::move(callback);

  platform_keys_service_->GetCertificates(
      GetPlatformKeysTokenId(cert_scope_),
      base::BindRepeating(
          &CertProvisioningCertsWithIdsGetter::OnGetCertificatesDone,
          weak_factory_.GetWeakPtr()));
}

void CertProvisioningCertsWithIdsGetter::OnGetCertificatesDone(
    std::unique_ptr<net::CertificateList> existing_certs,
    const std::string& error_message) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (!error_message.empty()) {
    std::move(callback_).Run(/*existing_cert_ids=*/{}, error_message);
    return;
  }

  if (!existing_certs || existing_certs->empty()) {
    std::move(callback_).Run(/*existing_cert_ids=*/{}, /*error_message=*/"");
    return;
  }

  wait_counter_ = existing_certs->size();

  for (const auto& cert : *existing_certs) {
    std::string public_key = platform_keys::GetSubjectPublicKeyInfo(cert);
    platform_keys_service_->GetAttributeForKey(
        GetPlatformKeysTokenId(cert_scope_), public_key,
        platform_keys::KeyAttributeType::CertificateProvisioningId,
        base::BindOnce(&CertProvisioningCertsWithIdsGetter::CollectOneResult,
                       weak_factory_.GetWeakPtr(), cert));
  }
}

void CertProvisioningCertsWithIdsGetter::CollectOneResult(
    scoped_refptr<net::X509Certificate> cert,
    const std::string& cert_id,
    const std::string& error_message) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(wait_counter_ > 0);

  // |callback_| could have been already used in case of an error in a parallel
  // task.
  if (callback_.is_null()) {
    return;
  }

  if (!error_message.empty()) {
    std::move(callback_).Run(/*existing_cert_ids=*/{}, error_message);
    return;
  }

  certs_with_ids_[cert_id] = cert;

  --wait_counter_;
  if (wait_counter_ != 0) {
    return;
  }

  // Move the result into the callback argument to allow deleting this object
  // before using the result.
  std::move(callback_).Run(std::move(certs_with_ids_), /*error_message=*/"");
}

// ========= CertProvisioningCertDeleter =======================================

CertProvisioningCertDeleter::CertProvisioningCertDeleter() = default;
CertProvisioningCertDeleter::~CertProvisioningCertDeleter() = default;

void CertProvisioningCertDeleter::DeleteCerts(
    CertScope cert_scope,
    platform_keys::PlatformKeysService* platform_keys_service,
    const std::set<std::string>& cert_profile_ids_to_keep,
    DeleteCertsCallback callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(platform_keys_service);
  DCHECK(callback_.is_null());

  cert_scope_ = cert_scope;
  platform_keys_service_ = platform_keys_service;
  callback_ = std::move(callback);
  cert_profile_ids_to_keep_ = cert_profile_ids_to_keep;

  cert_getter_ = std::make_unique<CertProvisioningCertsWithIdsGetter>();
  cert_getter_->GetCertsWithIds(
      cert_scope, platform_keys_service,
      base::BindOnce(&CertProvisioningCertDeleter::OnGetCertsWithIdsDone,
                     weak_factory_.GetWeakPtr()));
}

void CertProvisioningCertDeleter::OnGetCertsWithIdsDone(
    std::map<std::string, scoped_refptr<net::X509Certificate>> certs_with_ids,
    const std::string& error_message) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (!error_message.empty()) {
    std::move(callback_).Run(error_message);
    return;
  }

  if (certs_with_ids.empty()) {
    std::move(callback_).Run(/*error_message=*/"");
    return;
  }

  wait_counter_ = certs_with_ids.size();

  for (const auto& kv : certs_with_ids) {
    const std::string& cert_id = kv.first;
    if (base::Contains(cert_profile_ids_to_keep_, cert_id)) {
      AccountOneResult();
      continue;
    }

    const scoped_refptr<net::X509Certificate>& cert = kv.second;
    platform_keys_service_->RemoveCertificate(
        GetPlatformKeysTokenId(cert_scope_), cert,
        base::BindRepeating(
            &CertProvisioningCertDeleter::OnRemoveCertificateDone,
            weak_factory_.GetWeakPtr()));
  }
}

void CertProvisioningCertDeleter::OnRemoveCertificateDone(
    const std::string& error_message) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (!error_message.empty()) {
    std::move(callback_).Run(error_message);
    return;
  }

  AccountOneResult();
}

void CertProvisioningCertDeleter::AccountOneResult() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(wait_counter_ > 0);

  // |callback_| could have been already used in case of an error in a parallel
  // task.
  if (callback_.is_null()) {
    return;
  }

  --wait_counter_;
  if (wait_counter_ != 0) {
    return;
  }

  std::move(callback_).Run(/*error_message=*/"");
}

}  // namespace cert_provisioning
}  // namespace chromeos
