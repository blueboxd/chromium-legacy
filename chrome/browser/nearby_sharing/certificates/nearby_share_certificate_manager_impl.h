// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_NEARBY_SHARING_CERTIFICATES_NEARBY_SHARE_CERTIFICATE_MANAGER_IMPL_H_
#define CHROME_BROWSER_NEARBY_SHARING_CERTIFICATES_NEARBY_SHARE_CERTIFICATE_MANAGER_IMPL_H_

#include <memory>
#include <vector>

#include "base/containers/span.h"
#include "base/files/file_path.h"
#include "base/optional.h"
#include "base/time/clock.h"
#include "base/time/default_clock.h"
#include "chrome/browser/nearby_sharing/certificates/nearby_share_certificate_manager.h"
#include "chrome/browser/nearby_sharing/certificates/nearby_share_certificate_storage.h"
#include "chrome/browser/nearby_sharing/certificates/nearby_share_encrypted_metadata_key.h"
#include "chrome/browser/nearby_sharing/certificates/nearby_share_private_certificate.h"
#include "chrome/browser/nearby_sharing/certificates/nearby_share_visibility.h"
#include "chrome/browser/nearby_sharing/common/nearby_share_http_result.h"
#include "chrome/browser/nearby_sharing/proto/rpc_resources.pb.h"

class NearbyShareClient;
class NearbyShareClientFactory;
class NearbyShareLocalDeviceDataManager;
class NearbyShareScheduler;
class PrefService;

namespace leveldb_proto {
class ProtoDatabaseProvider;
}  // namespace leveldb_proto

namespace nearbyshare {
namespace proto {
class ListPublicCertificatesResponse;
}  // namespace proto
}  // namespace nearbyshare

// TODO(nohle): Add description after class is fully implemented.
class NearbyShareCertificateManagerImpl : public NearbyShareCertificateManager {
 public:
  class Factory {
   public:
    static std::unique_ptr<NearbyShareCertificateManager> Create(
        NearbyShareLocalDeviceDataManager* local_device_data_manager,
        PrefService* pref_service,
        leveldb_proto::ProtoDatabaseProvider* proto_database_provider,
        const base::FilePath& profile_path,
        NearbyShareClientFactory* client_factory,
        base::Clock* clock = base::DefaultClock::GetInstance());
    static void SetFactoryForTesting(Factory* test_factory);

   protected:
    virtual ~Factory();
    virtual std::unique_ptr<NearbyShareCertificateManager> CreateInstance(
        NearbyShareLocalDeviceDataManager* local_device_data_manager,
        PrefService* pref_service,
        leveldb_proto::ProtoDatabaseProvider* proto_database_provider,
        const base::FilePath& profile_path,
        NearbyShareClientFactory* client_factory,
        base::Clock* clock) = 0;

   private:
    static Factory* test_factory_;
  };

  ~NearbyShareCertificateManagerImpl() override;

 private:
  NearbyShareCertificateManagerImpl(
      NearbyShareLocalDeviceDataManager* local_device_data_manager,
      PrefService* pref_service,
      leveldb_proto::ProtoDatabaseProvider* proto_database_provider,
      const base::FilePath& profile_path,
      NearbyShareClientFactory* client_factory,
      base::Clock* clock);

  // NearbyShareCertificateManager:
  NearbySharePrivateCertificate GetValidPrivateCertificate(
      NearbyShareVisibility visibility) override;
  std::vector<nearbyshare::proto::PublicCertificate>
  GetPrivateCertificatesAsPublicCertificates(
      NearbyShareVisibility visibility) override;
  void GetDecryptedPublicCertificate(
      NearbyShareEncryptedMetadataKey encrypted_metadata_key,
      CertDecryptedCallback callback) override;
  void DownloadPublicCertificates() override;
  void OnStart() override;
  void OnStop() override;

  void OnDownloadPublicCertificatesRequest(
      base::Optional<std::string> page_token);
  void OnRpcSuccess(
      const nearbyshare::proto::ListPublicCertificatesResponse& response);
  void OnRpcFailure(NearbyShareHttpError error);
  void OnPublicCertificatesAdded(base::Optional<std::string> page_token,
                                 bool success);
  void FinishDownloadPublicCertificates(bool success,
                                        NearbyShareHttpResult http_result);

  NearbyShareLocalDeviceDataManager* local_device_data_manager_ = nullptr;
  PrefService* pref_service_ = nullptr;
  NearbyShareClientFactory* client_factory_ = nullptr;
  base::Clock* clock_ = nullptr;
  std::unique_ptr<NearbyShareScheduler> download_public_certificates_scheduler_;
  std::unique_ptr<NearbyShareCertificateStorage> cert_store_;
  std::unique_ptr<NearbyShareClient> client_;
};

#endif  // CHROME_BROWSER_NEARBY_SHARING_CERTIFICATES_NEARBY_SHARE_CERTIFICATE_MANAGER_IMPL_H_
