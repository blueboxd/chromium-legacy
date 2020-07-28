// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_NEARBY_SHARING_CERTIFICATES_NEARBY_SHARE_CERTIFICATE_STORAGE_IMPL_H_
#define CHROME_BROWSER_NEARBY_SHARING_CERTIFICATES_NEARBY_SHARE_CERTIFICATE_STORAGE_IMPL_H_

#include "base/containers/flat_set.h"
#include "chrome/browser/nearby_sharing/certificates/nearby_share_certificate_storage.h"
#include "components/leveldb_proto/public/proto_database.h"

class NearbySharePrivateCertificate;
class PrefRegistrySimple;
class PrefService;

namespace nearbyshare {
namespace proto {
class PublicCertificate;
}  // namespace proto
}  // namespace nearbyshare

// Implements NearbyShareCertificateStorage using Prefs to store private
// certificates and LevelDB Proto to store public certificates. Must be
// initialized by calling Initialize before retrieving or storing
// certificates.
class NearbyShareCertificateStorageImpl : public NearbyShareCertificateStorage {
 public:
  using ExpirationList = std::vector<std::pair<std::string, base::Time>>;

  // Registers the prefs used by this class to the given |registry|.
  static void RegisterPrefs(PrefRegistrySimple* registry);

  NearbyShareCertificateStorageImpl(
      PrefService* pref_service,
      std::unique_ptr<
          leveldb_proto::ProtoDatabase<nearbyshare::proto::PublicCertificate>>
          proto_database);
  ~NearbyShareCertificateStorageImpl() override;

  NearbyShareCertificateStorageImpl(NearbyShareCertificateStorageImpl&) =
      delete;
  void operator=(NearbyShareCertificateStorageImpl&) = delete;

  // NearbyShareCertificateStorage
  bool IsInitialized() override;
  void Initialize(ResultCallback callback) override;
  std::vector<std::string> GetPublicCertificateIds() const override;
  void GetPublicCertificates(PublicCertificateCallback callback) const override;
  base::Optional<std::vector<NearbySharePrivateCertificate>>
  GetPrivateCertificates() const override;
  base::Optional<base::Time> NextPrivateCertificateExpirationTime()
      const override;
  base::Optional<base::Time> NextPublicCertificateExpirationTime()
      const override;
  void ReplacePrivateCertificates(
      const std::vector<NearbySharePrivateCertificate>& private_certificates)
      override;
  void ReplacePublicCertificates(
      const std::vector<nearbyshare::proto::PublicCertificate>&
          public_certificates,
      ResultCallback callback) override;
  void AddPublicCertificates(
      const std::vector<nearbyshare::proto::PublicCertificate>&
          public_certificates,
      ResultCallback callback) override;
  void RemoveExpiredPublicCertificates(base::Time now,
                                       ResultCallback callback) override;
  void ClearPrivateCertificates() override;
  void ClearPublicCertificates(ResultCallback callback) override;

 private:
  void OnDatabaseInitialized(ResultCallback callback,
                             leveldb_proto::Enums::InitStatus status);

  void OnDatabaseDestroyed(bool should_reinitialize,
                           ResultCallback callback,
                           bool success);

  void DestroyAndReinitialize(ResultCallback callback);

  void ReplacePublicCertificatesDestroyCallback(
      std::unique_ptr<std::vector<
          std::pair<std::string, nearbyshare::proto::PublicCertificate>>>
          public_certificates,
      std::unique_ptr<ExpirationList> expirations,
      ResultCallback callback,
      bool proceed);
  void ReplacePublicCertificatesUpdateEntriesCallback(
      std::unique_ptr<ExpirationList> expirations,
      ResultCallback callback,
      bool proceed);
  void AddPublicCertificatesCallback(
      std::unique_ptr<ExpirationList> new_expirations,
      ResultCallback callback,
      bool proceed);
  void RemoveExpiredPublicCertificatesCallback(
      std::unique_ptr<base::flat_set<std::string>> ids_to_remove,
      ResultCallback callback,
      bool proceed);

  bool FetchPublicCertificateExpirations();
  void SavePublicCertificateExpirations();

  bool is_initialized_ = false;

  size_t num_initialize_attempts_ = 0;

  PrefService* pref_service_;

  std::unique_ptr<
      leveldb_proto::ProtoDatabase<nearbyshare::proto::PublicCertificate>>
      db_;

  std::vector<NearbySharePrivateCertificate> private_certificates_;
  ExpirationList public_certificate_expirations_;
};

#endif  // CHROME_BROWSER_NEARBY_SHARING_CERTIFICATES_NEARBY_SHARE_CERTIFICATE_STORAGE_IMPL_H_
