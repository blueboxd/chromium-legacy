// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/cert_provisioning/cert_provisioning_platform_keys_helpers.h"

#include <memory>

#include "base/bind.h"
#include "base/check.h"
#include "base/containers/flat_set.h"
#include "base/optional.h"
#include "base/stl_util.h"
#include "chrome/browser/chromeos/cert_provisioning/cert_provisioning_common.h"
#include "chrome/browser/chromeos/platform_keys/platform_keys_service.h"

namespace chromeos {
namespace cert_provisioning {

// ========= CertIterator ======================================================

CertIterator::CertIterator(
    CertScope cert_scope,
    platform_keys::PlatformKeysService* platform_keys_service)
    : cert_scope_(cert_scope), platform_keys_service_(platform_keys_service) {}
CertIterator::~CertIterator() = default;

void CertIterator::IterateAll(
    CertIteratorForEachCallback for_each_callback,
    CertIteratorOnFinishedCallback on_finished_callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  Cancel();
  for_each_callback_ = std::move(for_each_callback);
  on_finished_callback_ = std::move(on_finished_callback);

  platform_keys_service_->GetCertificates(
      GetPlatformKeysTokenId(cert_scope_),
      base::BindRepeating(&CertIterator::OnGetCertificatesDone,
                          weak_factory_.GetWeakPtr()));
}

void CertIterator::Cancel() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  weak_factory_.InvalidateWeakPtrs();
  for_each_callback_.Reset();
  on_finished_callback_.Reset();
}

void CertIterator::OnGetCertificatesDone(
    std::unique_ptr<net::CertificateList> existing_certs,
    const std::string& error_message) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (!error_message.empty()) {
    StopIteration(error_message);
    return;
  }

  // No work to do, return empty error message.
  if (!existing_certs || existing_certs->empty()) {
    StopIteration(error_message);
    return;
  }

  wait_counter_ = existing_certs->size();

  for (const auto& cert : *existing_certs) {
    std::string public_key = platform_keys::GetSubjectPublicKeyInfo(cert);
    platform_keys_service_->GetAttributeForKey(
        GetPlatformKeysTokenId(cert_scope_), public_key,
        platform_keys::KeyAttributeType::CertificateProvisioningId,
        base::BindOnce(&CertIterator::OnGetAttributeForKeyDone,
                       weak_factory_.GetWeakPtr(), cert));
  }
}

void CertIterator::OnGetAttributeForKeyDone(
    scoped_refptr<net::X509Certificate> cert,
    const base::Optional<std::string>& attr_value,
    const std::string& error_message) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(wait_counter_ > 0);

  // TODO(crbug.com/1073512): Currently if GetAttributeForKey fails to get the
  // attribute (because it was not set or any other reason), it will return
  // nullopt for cert_profile_id and empty error message. When
  // PlatformKeysService switches to error codes, a code for such situation
  // should not be returned via callback and cert collection can be continued.
  if (!error_message.empty()) {
    StopIteration(error_message);
    return;
  }

  if (attr_value) {
    for_each_callback_.Run(cert, attr_value.value(), /*error_message=*/"");
  }

  --wait_counter_;
  if (wait_counter_ == 0) {
    StopIteration(/*error_message=*/"");
  }
}

void CertIterator::StopIteration(const std::string& error_message) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(!on_finished_callback_.is_null());

  weak_factory_.InvalidateWeakPtrs();
  std::move(on_finished_callback_).Run(error_message);
}

// ========= LatestCertsWithIdsGetter ==========================================

LatestCertsWithIdsGetter::LatestCertsWithIdsGetter(
    CertScope cert_scope,
    platform_keys::PlatformKeysService* platform_keys_service)
    : iterator_(cert_scope, platform_keys_service) {}

LatestCertsWithIdsGetter::~LatestCertsWithIdsGetter() = default;

void LatestCertsWithIdsGetter::GetCertsWithIds(
    LatestCertsWithIdsGetterCallback callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  Cancel();
  callback_ = std::move(callback);

  iterator_.IterateAll(
      base::BindRepeating(&LatestCertsWithIdsGetter::ProcessOneCert,
                          weak_factory_.GetWeakPtr()),
      base::BindOnce(&LatestCertsWithIdsGetter::OnIterationFinished,
                     weak_factory_.GetWeakPtr()));
}

void LatestCertsWithIdsGetter::Cancel() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  weak_factory_.InvalidateWeakPtrs();
  callback_.Reset();
}

bool LatestCertsWithIdsGetter::IsRunning() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return !callback_.is_null();
}

void LatestCertsWithIdsGetter::ProcessOneCert(
    scoped_refptr<net::X509Certificate> new_cert,
    const CertProfileId& cert_profile_id,
    const std::string& error_message) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (!error_message.empty()) {
    OnIterationFinished(error_message);
    return;
  }

  auto cert_iter = certs_with_ids_.find(cert_profile_id);
  if (cert_iter == certs_with_ids_.end()) {
    certs_with_ids_[cert_profile_id] = new_cert;
    return;
  }

  const auto& existing_cert = cert_iter->second;
  if (existing_cert->valid_expiry() < new_cert->valid_expiry()) {
    cert_iter->second = new_cert;
    return;
  }
}

void LatestCertsWithIdsGetter::OnIterationFinished(
    const std::string& error_message) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(!callback_.is_null());

  weak_factory_.InvalidateWeakPtrs();

  if (!error_message.empty()) {
    certs_with_ids_ = {};
  }

  std::move(callback_).Run(std::move(certs_with_ids_), error_message);
}

// ========= CertDeleter =======================================================

CertDeleter::CertDeleter(
    CertScope cert_scope,
    platform_keys::PlatformKeysService* platform_keys_service)
    : cert_scope_(cert_scope),
      platform_keys_service_(platform_keys_service),
      iterator_(cert_scope, platform_keys_service) {}

CertDeleter::~CertDeleter() = default;

void CertDeleter::DeleteCerts(
    base::flat_set<CertProfileId> cert_profile_ids_to_keep,
    CertDeleterCallback callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  Cancel();
  callback_ = std::move(callback);
  cert_profile_ids_to_keep_ = std::move(cert_profile_ids_to_keep);

  iterator_.IterateAll(base::BindRepeating(&CertDeleter::ProcessOneCert,
                                           weak_factory_.GetWeakPtr()),
                       base::BindOnce(&CertDeleter::OnIterationFinished,
                                      weak_factory_.GetWeakPtr()));
}

void CertDeleter::Cancel() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  weak_factory_.InvalidateWeakPtrs();
  iteration_finished_ = false;
  pending_delete_tasks_counter_ = 0;
  callback_.Reset();
  certs_with_ids_.clear();
}

void CertDeleter::ProcessOneCert(scoped_refptr<net::X509Certificate> cert,
                                 const CertProfileId& cert_profile_id,
                                 const std::string& error_message) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (!error_message.empty()) {
    ReturnStatus(error_message);
    return;
  }

  RememberOrDelete(cert, cert_profile_id);
}

void CertDeleter::RememberOrDelete(scoped_refptr<net::X509Certificate> new_cert,
                                   const CertProfileId& cert_profile_id) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if ((!base::Contains(cert_profile_ids_to_keep_, cert_profile_id)) ||
      (base::Time::Now() > new_cert->valid_expiry())) {
    DeleteCert(new_cert);
    return;
  }

  auto cert_iter = certs_with_ids_.find(cert_profile_id);
  if (cert_iter == certs_with_ids_.end()) {
    certs_with_ids_[cert_profile_id] = new_cert;
    return;
  }

  // Keep only the newest certificate.
  const auto& existing_cert = cert_iter->second;
  if (existing_cert->valid_expiry() < new_cert->valid_expiry()) {
    DeleteCert(existing_cert);
    cert_iter->second = new_cert;
    return;
  } else {
    DeleteCert(new_cert);
    return;
  }
}

void CertDeleter::DeleteCert(scoped_refptr<net::X509Certificate> cert) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  ++pending_delete_tasks_counter_;
  platform_keys_service_->RemoveCertificate(
      GetPlatformKeysTokenId(cert_scope_), cert,
      base::BindRepeating(&CertDeleter::OnDeleteCertDone,
                          weak_factory_.GetWeakPtr()));
}

void CertDeleter::OnDeleteCertDone(const std::string& error_message) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(pending_delete_tasks_counter_ > 0);

  if (!error_message.empty()) {
    ReturnStatus(error_message);
    return;
  }

  --pending_delete_tasks_counter_;
  CheckStateAndMaybeFinish();
}

void CertDeleter::OnIterationFinished(const std::string& error_message) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  iteration_finished_ = true;
  CheckStateAndMaybeFinish();
}

void CertDeleter::CheckStateAndMaybeFinish() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (!iteration_finished_ || (pending_delete_tasks_counter_ > 0)) {
    return;
  }

  ReturnStatus(/*error_message=*/"");
}

void CertDeleter::ReturnStatus(const std::string& error_message) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(!callback_.is_null());

  weak_factory_.InvalidateWeakPtrs();
  std::move(callback_).Run(error_message);
}

}  // namespace cert_provisioning
}  // namespace chromeos
