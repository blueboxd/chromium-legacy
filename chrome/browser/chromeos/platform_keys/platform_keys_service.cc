// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/platform_keys/platform_keys_service.h"

#include <map>
#include <utility>

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/location.h"
#include "base/task/post_task.h"
#include "base/task/thread_pool.h"
#include "content/public/browser/browser_thread.h"
#include "net/base/hash_value.h"
#include "net/base/net_errors.h"
#include "net/cert/x509_certificate.h"

namespace chromeos {
namespace platform_keys {

namespace {

void IntersectOnWorkerThread(const net::CertificateList& certs1,
                             const net::CertificateList& certs2,
                             net::CertificateList* intersection) {
  std::map<net::SHA256HashValue, scoped_refptr<net::X509Certificate>>
      fingerprints2;

  // Fill the map with fingerprints of certs from |certs2|.
  for (const auto& cert2 : certs2) {
    fingerprints2[net::X509Certificate::CalculateFingerprint256(
        cert2->cert_buffer())] = cert2;
  }

  // Compare each cert from |certs1| with the entries of the map.
  for (const auto& cert1 : certs1) {
    const net::SHA256HashValue fingerprint1 =
        net::X509Certificate::CalculateFingerprint256(cert1->cert_buffer());
    const auto it = fingerprints2.find(fingerprint1);
    if (it == fingerprints2.end())
      continue;
    const auto& cert2 = it->second;
    DCHECK(cert1->EqualsExcludingChain(cert2.get()));
    intersection->push_back(cert1);
  }
}

}  // namespace

std::string StatusToString(Status status) {
  switch (status) {
    case Status::kSuccess:
      return "The operation was successfully executed.";
    case Status::kErrorAlgorithmNotSupported:
      return "Algorithm not supported.";
    case Status::kErrorCertificateNotFound:
      return "Certificate could not be found.";
    case Status::kErrorInternal:
      return "Internal Error.";
    case Status::kErrorKeyAttributeRetrievalFailed:
      return "Key attribute value retrieval failed.";
    case Status::kErrorKeyAttributeSettingFailed:
      return "Setting key attribute value failed.";
    case Status::kErrorKeyNotAllowedForSigning:
      return "This key is not allowed for signing. Either it was used for "
             "signing before or it was not correctly generated.";
    case Status::kErrorKeyNotFound:
      return "Key not found.";
    case Status::kErrorShutDown:
      return "Delegate shut down.";
    case Status::kNetErrorAddUserCertFailed:
      return net::ErrorToString(net::ERR_ADD_USER_CERT_FAILED);
    case Status::kNetErrorCertificateDateInvalid:
      return net::ErrorToString(net::ERR_CERT_DATE_INVALID);
    case Status::kNetErrorCertificateInvalid:
      return net::ErrorToString(net::ERR_CERT_INVALID);
  }
}

void IntersectCertificates(
    const net::CertificateList& certs1,
    const net::CertificateList& certs2,
    const base::Callback<void(std::unique_ptr<net::CertificateList>)>&
        callback) {
  std::unique_ptr<net::CertificateList> intersection(new net::CertificateList);
  net::CertificateList* const intersection_ptr = intersection.get();

  // This is triggered by a call to the
  // chrome.platformKeys.selectClientCertificates extensions API. Completion
  // does not affect browser responsiveness, hence the BEST_EFFORT priority.
  base::ThreadPool::PostTaskAndReply(
      FROM_HERE,
      {base::TaskPriority::BEST_EFFORT,
       base::TaskShutdownBehavior::CONTINUE_ON_SHUTDOWN},
      base::BindOnce(&IntersectOnWorkerThread, certs1, certs2,
                     intersection_ptr),
      base::BindOnce(callback, base::Passed(&intersection)));
}

// =================== ClientCertificateRequest ================================

ClientCertificateRequest::ClientCertificateRequest() = default;

ClientCertificateRequest::ClientCertificateRequest(
    const ClientCertificateRequest& other) = default;

ClientCertificateRequest::~ClientCertificateRequest() = default;

// =============== PlatformKeysServiceImplDelegate =============================

PlatformKeysServiceImplDelegate::PlatformKeysServiceImplDelegate() = default;

PlatformKeysServiceImplDelegate::~PlatformKeysServiceImplDelegate() {
  ShutDown();
}

void PlatformKeysServiceImplDelegate::SetOnShutdownCallback(
    base::OnceClosure on_shutdown_callback) {
  DCHECK(!shut_down_);
  DCHECK(!on_shutdown_callback_);
  on_shutdown_callback_ = std::move(on_shutdown_callback);
}

bool PlatformKeysServiceImplDelegate::IsShutDown() const {
  return shut_down_;
}

void PlatformKeysServiceImplDelegate::ShutDown() {
  if (shut_down_)
    return;

  shut_down_ = true;
  if (on_shutdown_callback_)
    std::move(on_shutdown_callback_).Run();
}

// =================== PlatformKeysServiceImpl =================================

PlatformKeysServiceImpl::PlatformKeysServiceImpl(
    std::unique_ptr<PlatformKeysServiceImplDelegate> delegate)
    : delegate_(std::move(delegate)) {
  // base::Unretained is OK because |delegate_| is owned by this and can
  // only call the callback before it is destroyed.
  delegate_->SetOnShutdownCallback(base::BindOnce(
      &PlatformKeysServiceImpl::OnDelegateShutDown, base::Unretained(this)));
}

PlatformKeysServiceImpl::~PlatformKeysServiceImpl() = default;

void PlatformKeysServiceImpl::AddObserver(
    PlatformKeysServiceObserver* observer) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  observers_.AddObserver(observer);
}

void PlatformKeysServiceImpl::RemoveObserver(
    PlatformKeysServiceObserver* observer) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  observers_.RemoveObserver(observer);
}

void PlatformKeysServiceImpl::OnDelegateShutDown() {
  for (auto& observer : observers_) {
    observer.OnPlatformKeysServiceShutDown();
  }
}

// The rest of the methods - the NSS-specific part of the implementation -
// resides in the platform_keys_service_nss.cc file.

}  // namespace platform_keys
}  // namespace chromeos
