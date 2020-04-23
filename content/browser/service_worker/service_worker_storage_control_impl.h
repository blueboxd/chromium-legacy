// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_SERVICE_WORKER_SERVICE_WORKER_STORAGE_CONTROL_IMPL_H_
#define CONTENT_BROWSER_SERVICE_WORKER_SERVICE_WORKER_STORAGE_CONTROL_IMPL_H_

#include <memory>

#include "components/services/storage/public/mojom/service_worker_storage_control.mojom.h"
#include "content/common/content_export.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"

namespace content {

class ServiceWorkerStorage;

// This class wraps ServiceWorkerStorage to implement mojo interface defined by
// the storage service, i.e., ServiceWorkerStorageControl.
// TODO(crbug.com/1055677): Merge this implementation into ServiceWorkerStorage
// and move the merged class to components/services/storage.
class CONTENT_EXPORT ServiceWorkerStorageControlImpl
    : public storage::mojom::ServiceWorkerStorageControl {
 public:
  explicit ServiceWorkerStorageControlImpl(
      std::unique_ptr<ServiceWorkerStorage> storage);

  ServiceWorkerStorageControlImpl(const ServiceWorkerStorageControlImpl&) =
      delete;
  ServiceWorkerStorageControlImpl& operator=(
      const ServiceWorkerStorageControlImpl&) = delete;

  ~ServiceWorkerStorageControlImpl() override;

  void LazyInitializeForTest();

 private:
  // storage::mojom::ServiceWorkerStorageControl implementations:
  void FindRegistrationForClientUrl(
      const GURL& client_url,
      FindRegistrationForClientUrlCallback callback) override;
  void FindRegistrationForScope(
      const GURL& scope,
      FindRegistrationForScopeCallback callback) override;
  void FindRegistrationForId(int64_t registration_id,
                             const GURL& origin,
                             FindRegistrationForIdCallback callback) override;
  void GetRegistrationsForOrigin(
      const GURL& origin,
      GetRegistrationsForOriginCallback callback) override;
  void StoreRegistration(
      storage::mojom::ServiceWorkerRegistrationDataPtr registration,
      std::vector<storage::mojom::ServiceWorkerResourceRecordPtr> resources,
      StoreRegistrationCallback callback) override;
  void DeleteRegistration(int64_t registration_id,
                          const GURL& origin,
                          DeleteRegistrationCallback callback) override;
  void UpdateToActiveState(int64_t registration_id,
                           const GURL& origin,
                           UpdateToActiveStateCallback callback) override;
  void UpdateLastUpdateCheckTime(
      int64_t registration_id,
      const GURL& origin,
      base::Time last_update_check_time,
      UpdateLastUpdateCheckTimeCallback callback) override;
  void UpdateNavigationPreloadEnabled(
      int64_t registration_id,
      const GURL& origin,
      bool enable,
      UpdateNavigationPreloadEnabledCallback callback) override;
  void UpdateNavigationPreloadHeader(
      int64_t registration_id,
      const GURL& origin,
      const std::string& value,
      UpdateNavigationPreloadHeaderCallback callback) override;
  void GetNewRegistrationId(GetNewRegistrationIdCallback callback) override;
  void GetNewVersionId(GetNewVersionIdCallback callback) override;
  void GetNewResourceId(GetNewResourceIdCallback callback) override;
  void CreateResourceReader(
      int64_t resource_id,
      mojo::PendingReceiver<storage::mojom::ServiceWorkerResourceReader> reader)
      override;
  void CreateResourceWriter(
      int64_t resource_id,
      mojo::PendingReceiver<storage::mojom::ServiceWorkerResourceWriter> writer)
      override;
  void CreateResourceMetadataWriter(
      int64_t resource_id,
      mojo::PendingReceiver<storage::mojom::ServiceWorkerResourceMetadataWriter>
          writer) override;
  void GetUserData(int64_t registration_id,
                   const std::vector<std::string>& keys,
                   GetUserDataCallback callback) override;
  void StoreUserData(
      int64_t registration_id,
      const GURL& origin,
      std::vector<storage::mojom::ServiceWorkerUserDataPtr> user_data,
      StoreUserDataCallback callback) override;
  void ClearUserData(int64_t registration_id,
                     const std::vector<std::string>& keys,
                     ClearUserDataCallback callback) override;
  void GetUserDataByKeyPrefix(int64_t registration_id,
                              const std::string& key_prefix,
                              GetUserDataByKeyPrefixCallback callback) override;
  void GetUserKeysAndDataByKeyPrefix(
      int64_t registration_id,
      const std::string& key_prefix,
      GetUserKeysAndDataByKeyPrefixCallback callback) override;
  void ClearUserDataByKeyPrefixes(
      int64_t registration_id,
      const std::vector<std::string>& key_prefixes,
      ClearUserDataByKeyPrefixesCallback callback) override;
  void GetUserDataForAllRegistrations(
      const std::string& key,
      GetUserDataForAllRegistrationsCallback callback) override;
  void GetUserDataForAllRegistrationsByKeyPrefix(
      const std::string& key_prefix,
      GetUserDataForAllRegistrationsByKeyPrefixCallback callback) override;
  void ClearUserDataForAllRegistrationsByKeyPrefix(
      const std::string& key_prefix,
      ClearUserDataForAllRegistrationsByKeyPrefixCallback callback) override;
  void ApplyPolicyUpdates(
      const std::vector<storage::mojom::LocalStoragePolicyUpdatePtr>
          policy_updates) override;

  const std::unique_ptr<ServiceWorkerStorage> storage_;
};

}  // namespace content

#endif  // CONTENT_BROWSER_SERVICE_WORKER_SERVICE_WORKER_STORAGE_CONTROLIMPL_H_
