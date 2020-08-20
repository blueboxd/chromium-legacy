// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/nearby_sharing/certificates/nearby_share_certificate_manager_impl.h"

#include "base/bind.h"
#include "base/callback.h"
#include "base/memory/ptr_util.h"
#include "base/notreached.h"
#include "base/time/time.h"
#include "chrome/browser/nearby_sharing/certificates/common.h"
#include "chrome/browser/nearby_sharing/certificates/constants.h"
#include "chrome/browser/nearby_sharing/certificates/nearby_share_certificate_storage_impl.h"
#include "chrome/browser/nearby_sharing/client/nearby_share_client.h"
#include "chrome/browser/nearby_sharing/common/nearby_share_http_result.h"
#include "chrome/browser/nearby_sharing/common/nearby_share_prefs.h"
#include "chrome/browser/nearby_sharing/local_device_data/nearby_share_local_device_data_manager.h"
#include "chrome/browser/nearby_sharing/proto/certificate_rpc.pb.h"
#include "chrome/browser/nearby_sharing/proto/encrypted_metadata.pb.h"
#include "chrome/browser/nearby_sharing/scheduling/nearby_share_scheduler_factory.h"
#include "components/leveldb_proto/public/proto_database_provider.h"
#include "components/prefs/pref_service.h"

namespace {

const char kDeviceIdPrefix[] = "users/me/devices/";

void TryDecryptPublicCertificates(
    const NearbyShareEncryptedMetadataKey& encrypted_metadata_key,
    NearbyShareCertificateManager::CertDecryptedCallback callback,
    bool success,
    std::unique_ptr<std::vector<nearbyshare::proto::PublicCertificate>>
        public_certificates) {
  if (!success || !public_certificates) {
    std::move(callback).Run(base::nullopt);
    return;
  }

  for (const auto& cert : *public_certificates) {
    base::Optional<NearbyShareDecryptedPublicCertificate> decrypted =
        NearbyShareDecryptedPublicCertificate::DecryptPublicCertificate(
            cert, encrypted_metadata_key);
    if (decrypted) {
      std::move(callback).Run(std::move(decrypted));
      return;
    }
  }
  std::move(callback).Run(base::nullopt);
}

void RecordResultMetrics(NearbyShareHttpResult result) {
  // TODO(crbug.com/1105579): Record a histogram value for each result.
}

}  // namespace

// static
NearbyShareCertificateManagerImpl::Factory*
    NearbyShareCertificateManagerImpl::Factory::test_factory_ = nullptr;

// static
std::unique_ptr<NearbyShareCertificateManager>
NearbyShareCertificateManagerImpl::Factory::Create(
    NearbyShareLocalDeviceDataManager* local_device_data_manager,
    PrefService* pref_service,
    leveldb_proto::ProtoDatabaseProvider* proto_database_provider,
    const base::FilePath& profile_path,
    NearbyShareClientFactory* client_factory,
    base::Clock* clock) {
  DCHECK(clock);

  if (test_factory_) {
    return test_factory_->CreateInstance(local_device_data_manager,
                                         pref_service, proto_database_provider,
                                         profile_path, client_factory, clock);
  }

  return base::WrapUnique(new NearbyShareCertificateManagerImpl(
      local_device_data_manager, pref_service, proto_database_provider,
      profile_path, client_factory, clock));
}

// static
void NearbyShareCertificateManagerImpl::Factory::SetFactoryForTesting(
    Factory* test_factory) {
  test_factory_ = test_factory;
}

NearbyShareCertificateManagerImpl::Factory::~Factory() = default;

NearbyShareCertificateManagerImpl::NearbyShareCertificateManagerImpl(
    NearbyShareLocalDeviceDataManager* local_device_data_manager,
    PrefService* pref_service,
    leveldb_proto::ProtoDatabaseProvider* proto_database_provider,
    const base::FilePath& profile_path,
    NearbyShareClientFactory* client_factory,
    base::Clock* clock)
    : local_device_data_manager_(local_device_data_manager),
      pref_service_(pref_service),
      client_factory_(client_factory),
      clock_(clock),
      download_public_certificates_scheduler_(
          NearbyShareSchedulerFactory::CreatePeriodicScheduler(
              kNearbySharePublicCertificateDownloadPeriod,
              /*retry_failures=*/true,
              /*require_connectivity=*/true,
              prefs::kNearbySharingSchedulerDownloadPublicCertificatesPrefName,
              pref_service_,
              base::BindRepeating(&NearbyShareCertificateManagerImpl::
                                      OnDownloadPublicCertificatesRequest,
                                  base::Unretained(this),
                                  /*page_token=*/base::nullopt),
              clock_)),
      cert_store_(NearbyShareCertificateStorageImpl::Factory::Create(
          pref_service_,
          proto_database_provider,
          profile_path)) {}
NearbyShareCertificateManagerImpl::~NearbyShareCertificateManagerImpl() =
    default;

NearbySharePrivateCertificate
NearbyShareCertificateManagerImpl::GetValidPrivateCertificate(
    NearbyShareVisibility visibility) {
  base::Optional<std::vector<NearbySharePrivateCertificate>> certs =
      cert_store_->GetPrivateCertificates();
  DCHECK(certs);
  for (auto& cert : *certs) {
    if (IsNearbyShareCertificateWithinValidityPeriod(
            clock_->Now(), cert.not_before(), cert.not_after(),
            /*use_public_certificate_tolerance=*/false) &&
        cert.visibility() == visibility) {
      return std::move(cert);
    }
  }
  NOTREACHED();
  return NearbySharePrivateCertificate(NearbyShareVisibility::kNoOne,
                                       /*not_before=*/base::Time(),
                                       nearbyshare::proto::EncryptedMetadata());
}

std::vector<nearbyshare::proto::PublicCertificate>
NearbyShareCertificateManagerImpl::GetPrivateCertificatesAsPublicCertificates(
    NearbyShareVisibility visibility) {
  NOTIMPLEMENTED();
  return std::vector<nearbyshare::proto::PublicCertificate>();
}

void NearbyShareCertificateManagerImpl::GetDecryptedPublicCertificate(
    NearbyShareEncryptedMetadataKey encrypted_metadata_key,
    CertDecryptedCallback callback) {
  cert_store_->GetPublicCertificates(
      base::BindOnce(&TryDecryptPublicCertificates,
                     std::move(encrypted_metadata_key), std::move(callback)));
}

void NearbyShareCertificateManagerImpl::DownloadPublicCertificates() {
  download_public_certificates_scheduler_->MakeImmediateRequest();
}

void NearbyShareCertificateManagerImpl::OnDownloadPublicCertificatesRequest(
    base::Optional<std::string> page_token) {
  DCHECK(!client_);

  nearbyshare::proto::ListPublicCertificatesRequest request;
  request.set_parent(kDeviceIdPrefix + local_device_data_manager_->GetId());
  if (page_token)
    request.set_page_token(*page_token);

  for (const std::string& id : cert_store_->GetPublicCertificateIds()) {
    request.add_secret_ids(id);
  }

  // TODO(https://crbug.com/1116910): Enforce a timeout for each
  // ListPublicCertificates call.
  client_ = client_factory_->CreateInstance();
  client_->ListPublicCertificates(
      request,
      base::BindOnce(&NearbyShareCertificateManagerImpl::OnRpcSuccess,
                     base::Unretained(this)),
      base::BindOnce(&NearbyShareCertificateManagerImpl::OnRpcFailure,
                     base::Unretained(this)));
}

void NearbyShareCertificateManagerImpl::OnRpcSuccess(
    const nearbyshare::proto::ListPublicCertificatesResponse& response) {
  std::vector<nearbyshare::proto::PublicCertificate> certs(
      response.public_certificates().begin(),
      response.public_certificates().end());

  base::Optional<std::string> page_token =
      response.next_page_token().empty()
          ? base::nullopt
          : base::make_optional(response.next_page_token());

  client_.reset();

  cert_store_->AddPublicCertificates(
      certs, base::BindOnce(
                 &NearbyShareCertificateManagerImpl::OnPublicCertificatesAdded,
                 base::Unretained(this), page_token));
}

void NearbyShareCertificateManagerImpl::OnRpcFailure(
    NearbyShareHttpError error) {
  client_.reset();

  FinishDownloadPublicCertificates(/*success=*/false,
                                   NearbyShareHttpErrorToResult(error));
}

void NearbyShareCertificateManagerImpl::OnPublicCertificatesAdded(
    base::Optional<std::string> page_token,
    bool success) {
  if (success && page_token) {
    OnDownloadPublicCertificatesRequest(page_token);
  } else {
    FinishDownloadPublicCertificates(success, NearbyShareHttpResult::kSuccess);
  }
}

void NearbyShareCertificateManagerImpl::FinishDownloadPublicCertificates(
    bool success,
    NearbyShareHttpResult http_result) {
  RecordResultMetrics(http_result);
  download_public_certificates_scheduler_->HandleResult(success);
}

void NearbyShareCertificateManagerImpl::OnStart() {
  download_public_certificates_scheduler_->Start();
}

void NearbyShareCertificateManagerImpl::OnStop() {
  download_public_certificates_scheduler_->Stop();
}
