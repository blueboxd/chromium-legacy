// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/service_worker/service_worker_storage_control_impl.h"

#include "content/browser/service_worker/service_worker_resource_ops.h"
#include "content/browser/service_worker/service_worker_storage.h"
#include "mojo/public/cpp/bindings/self_owned_receiver.h"

namespace content {

namespace {

using ResourceList =
    std::vector<storage::mojom::ServiceWorkerResourceRecordPtr>;

void DidFindRegistration(
    base::OnceCallback<
        void(storage::mojom::ServiceWorkerFindRegistrationResultPtr)> callback,
    storage::mojom::ServiceWorkerRegistrationDataPtr data,
    std::unique_ptr<ResourceList> resources,
    storage::mojom::ServiceWorkerDatabaseStatus status) {
  ResourceList resource_list =
      resources ? std::move(*resources) : ResourceList();
  std::move(callback).Run(
      storage::mojom::ServiceWorkerFindRegistrationResult::New(
          status, std::move(data), std::move(resource_list)));
}

void DidStoreRegistration(
    ServiceWorkerStorageControlImpl::StoreRegistrationCallback callback,
    storage::mojom::ServiceWorkerDatabaseStatus status,
    int64_t deleted_version_id,
    const std::vector<int64_t>& newly_purgeable_resources) {
  // TODO(bashi): Figure out how to purge resources.
  std::move(callback).Run(status);
}

void DidDeleteRegistration(
    ServiceWorkerStorageControlImpl::DeleteRegistrationCallback callback,
    storage::mojom::ServiceWorkerDatabaseStatus status,
    ServiceWorkerStorage::OriginState origin_state,
    int64_t deleted_version_id,
    const std::vector<int64_t>& newly_purgeable_resources) {
  // TODO(bashi): Figure out how to purge resources.
  std::move(callback).Run(status, origin_state);
}

void DidGetRegistrationsForOrigin(
    ServiceWorkerStorageControlImpl::GetRegistrationsForOriginCallback callback,
    storage::mojom::ServiceWorkerDatabaseStatus status,
    std::unique_ptr<ServiceWorkerStorage::RegistrationList>
        registration_data_list,
    std::unique_ptr<std::vector<ServiceWorkerStorage::ResourceList>>
        resources_list) {
  DCHECK_EQ(registration_data_list->size(), resources_list->size());

  std::vector<storage::mojom::SerializedServiceWorkerRegistrationPtr>
      registrations;
  for (size_t i = 0; i < registration_data_list->size(); ++i) {
    registrations.push_back(
        storage::mojom::SerializedServiceWorkerRegistration::New(
            std::move((*registration_data_list)[i]),
            std::move((*resources_list)[i])));
  }

  std::move(callback).Run(status, std::move(registrations));
}

void DidGetUserData(
    ServiceWorkerStorageControlImpl::GetUserDataCallback callback,
    const std::vector<std::string>& values,
    storage::mojom::ServiceWorkerDatabaseStatus status) {
  // TODO(bashi): Change ServiceWorkerStorage::GetUserDataInDBCallback to remove
  // this indirection (the order of |values| and |status| is different).
  std::move(callback).Run(status, values);
}

void DidGetKeysAndUserData(
    ServiceWorkerStorageControlImpl::GetUserKeysAndDataByKeyPrefixCallback
        callback,
    const base::flat_map<std::string, std::string>& user_data,
    storage::mojom::ServiceWorkerDatabaseStatus status) {
  // TODO(bashi): Change ServiceWorkerStorage::GetUserKeysAndDataInDBCallback to
  // remove this indirection (the order of |user_data| and |status| is
  // different).
  std::move(callback).Run(status, user_data);
}

void DidGetUserDataForAllRegistrations(
    ServiceWorkerStorageControlImpl::GetUserDataForAllRegistrationsCallback
        callback,
    const std::vector<std::pair<int64_t, std::string>>& user_data,
    storage::mojom::ServiceWorkerDatabaseStatus status) {
  // TODO(bashi): Change ServiceWorkerStorage::GetUserDataForAllRegistrations()
  // to return base::flat_map.
  base::flat_map<int64_t, std::string> values;
  for (auto& entry : user_data) {
    values[entry.first] = entry.second;
  }
  std::move(callback).Run(status, std::move(values));
}

}  // namespace

ServiceWorkerStorageControlImpl::ServiceWorkerStorageControlImpl(
    std::unique_ptr<ServiceWorkerStorage> storage)
    : storage_(std::move(storage)) {
  DCHECK(storage_);
}

ServiceWorkerStorageControlImpl::~ServiceWorkerStorageControlImpl() = default;

void ServiceWorkerStorageControlImpl::LazyInitializeForTest() {
  storage_->LazyInitializeForTest();
}

void ServiceWorkerStorageControlImpl::FindRegistrationForClientUrl(
    const GURL& client_url,
    FindRegistrationForClientUrlCallback callback) {
  storage_->FindRegistrationForClientUrl(
      client_url, base::BindOnce(&DidFindRegistration, std::move(callback)));
}

void ServiceWorkerStorageControlImpl::FindRegistrationForScope(
    const GURL& scope,
    FindRegistrationForClientUrlCallback callback) {
  storage_->FindRegistrationForScope(
      scope, base::BindOnce(&DidFindRegistration, std::move(callback)));
}

void ServiceWorkerStorageControlImpl::FindRegistrationForId(
    int64_t registration_id,
    const GURL& origin,
    FindRegistrationForClientUrlCallback callback) {
  storage_->FindRegistrationForId(
      registration_id, origin,
      base::BindOnce(&DidFindRegistration, std::move(callback)));
}

void ServiceWorkerStorageControlImpl::GetRegistrationsForOrigin(
    const GURL& origin,
    GetRegistrationsForOriginCallback callback) {
  storage_->GetRegistrationsForOrigin(
      origin,
      base::BindOnce(&DidGetRegistrationsForOrigin, std::move(callback)));
}

void ServiceWorkerStorageControlImpl::StoreRegistration(
    storage::mojom::ServiceWorkerRegistrationDataPtr registration,
    std::vector<storage::mojom::ServiceWorkerResourceRecordPtr> resources,
    StoreRegistrationCallback callback) {
  // TODO(bashi): Change the signature of
  // ServiceWorkerStorage::StoreRegistrationData() to take a const reference.
  storage_->StoreRegistrationData(
      std::move(registration),
      std::make_unique<ServiceWorkerStorage::ResourceList>(
          std::move(resources)),
      base::BindOnce(&DidStoreRegistration, std::move(callback)));
}

void ServiceWorkerStorageControlImpl::DeleteRegistration(
    int64_t registration_id,
    const GURL& origin,
    DeleteRegistrationCallback callback) {
  storage_->DeleteRegistration(
      registration_id, origin,
      base::BindOnce(&DidDeleteRegistration, std::move(callback)));
}

void ServiceWorkerStorageControlImpl::UpdateToActiveState(
    int64_t registration_id,
    const GURL& origin,
    UpdateToActiveStateCallback callback) {
  storage_->UpdateToActiveState(registration_id, origin, std::move(callback));
}

void ServiceWorkerStorageControlImpl::UpdateLastUpdateCheckTime(
    int64_t registration_id,
    const GURL& origin,
    base::Time last_update_check_time,
    UpdateLastUpdateCheckTimeCallback callback) {
  storage_->UpdateLastUpdateCheckTime(
      registration_id, origin, last_update_check_time, std::move(callback));
}

void ServiceWorkerStorageControlImpl::UpdateNavigationPreloadEnabled(
    int64_t registration_id,
    const GURL& origin,
    bool enable,
    UpdateNavigationPreloadEnabledCallback callback) {
  storage_->UpdateNavigationPreloadEnabled(registration_id, origin, enable,
                                           std::move(callback));
}

void ServiceWorkerStorageControlImpl::UpdateNavigationPreloadHeader(
    int64_t registration_id,
    const GURL& origin,
    const std::string& value,
    UpdateNavigationPreloadHeaderCallback callback) {
  storage_->UpdateNavigationPreloadHeader(registration_id, origin, value,
                                          std::move(callback));
}

void ServiceWorkerStorageControlImpl::GetNewRegistrationId(
    GetNewRegistrationIdCallback callback) {
  storage_->GetNewRegistrationId(std::move(callback));
}

void ServiceWorkerStorageControlImpl::GetNewVersionId(
    GetNewVersionIdCallback callback) {
  storage_->GetNewVersionId(std::move(callback));
}

void ServiceWorkerStorageControlImpl::GetNewResourceId(
    GetNewResourceIdCallback callback) {
  storage_->GetNewResourceId(std::move(callback));
}

void ServiceWorkerStorageControlImpl::CreateResourceReader(
    int64_t resource_id,
    mojo::PendingReceiver<storage::mojom::ServiceWorkerResourceReader> reader) {
  DCHECK_NE(resource_id, blink::mojom::kInvalidServiceWorkerResourceId);
  mojo::MakeSelfOwnedReceiver(std::make_unique<ServiceWorkerResourceReaderImpl>(
                                  storage_->CreateResponseReader(resource_id)),
                              std::move(reader));
}

void ServiceWorkerStorageControlImpl::CreateResourceWriter(
    int64_t resource_id,
    mojo::PendingReceiver<storage::mojom::ServiceWorkerResourceWriter> writer) {
  DCHECK_NE(resource_id, blink::mojom::kInvalidServiceWorkerResourceId);
  mojo::MakeSelfOwnedReceiver(std::make_unique<ServiceWorkerResourceWriterImpl>(
                                  storage_->CreateResponseWriter(resource_id)),
                              std::move(writer));
}

void ServiceWorkerStorageControlImpl::CreateResourceMetadataWriter(
    int64_t resource_id,
    mojo::PendingReceiver<storage::mojom::ServiceWorkerResourceMetadataWriter>
        writer) {
  DCHECK_NE(resource_id, blink::mojom::kInvalidServiceWorkerResourceId);
  mojo::MakeSelfOwnedReceiver(
      std::make_unique<ServiceWorkerResourceMetadataWriterImpl>(
          storage_->CreateResponseMetadataWriter(resource_id)),
      std::move(writer));
}

void ServiceWorkerStorageControlImpl::GetUserData(
    int64_t registration_id,
    const std::vector<std::string>& keys,
    GetUserDataCallback callback) {
  storage_->GetUserData(registration_id, keys,
                        base::BindOnce(&DidGetUserData, std::move(callback)));
}

void ServiceWorkerStorageControlImpl::StoreUserData(
    int64_t registration_id,
    const GURL& origin,
    std::vector<storage::mojom::ServiceWorkerUserDataPtr> user_data,
    StoreUserDataCallback callback) {
  // TODO(bashi): Change ServiceWorkerStorage::StoreUserData to take
  // |user_data| so that we don't need to convert it.
  std::vector<std::pair<std::string, std::string>> key_value_pairs;
  for (auto& entry : user_data) {
    key_value_pairs.push_back(
        std::make_pair(std::move(entry->key), std::move(entry->value)));
  }
  storage_->StoreUserData(registration_id, origin, std::move(key_value_pairs),
                          std::move(callback));
}

void ServiceWorkerStorageControlImpl::ClearUserData(
    int64_t registration_id,
    const std::vector<std::string>& keys,
    ClearUserDataCallback callback) {
  storage_->ClearUserData(registration_id, keys, std::move(callback));
}

void ServiceWorkerStorageControlImpl::GetUserDataByKeyPrefix(
    int64_t registration_id,
    const std::string& key_prefix,
    GetUserDataByKeyPrefixCallback callback) {
  storage_->GetUserDataByKeyPrefix(
      registration_id, key_prefix,
      base::BindOnce(&DidGetUserData, std::move(callback)));
}

void ServiceWorkerStorageControlImpl::GetUserKeysAndDataByKeyPrefix(
    int64_t registration_id,
    const std::string& key_prefix,
    GetUserKeysAndDataByKeyPrefixCallback callback) {
  storage_->GetUserKeysAndDataByKeyPrefix(
      registration_id, key_prefix,
      base::BindOnce(&DidGetKeysAndUserData, std::move(callback)));
}

void ServiceWorkerStorageControlImpl::ClearUserDataByKeyPrefixes(
    int64_t registration_id,
    const std::vector<std::string>& key_prefixes,
    ClearUserDataByKeyPrefixesCallback callback) {
  storage_->ClearUserDataByKeyPrefixes(registration_id, key_prefixes,
                                       std::move(callback));
}

void ServiceWorkerStorageControlImpl::GetUserDataForAllRegistrations(
    const std::string& key,
    GetUserDataForAllRegistrationsCallback callback) {
  storage_->GetUserDataForAllRegistrations(
      key,
      base::BindOnce(&DidGetUserDataForAllRegistrations, std::move(callback)));
}

void ServiceWorkerStorageControlImpl::GetUserDataForAllRegistrationsByKeyPrefix(
    const std::string& key_prefix,
    GetUserDataForAllRegistrationsByKeyPrefixCallback callback) {
  storage_->GetUserDataForAllRegistrationsByKeyPrefix(
      key_prefix,
      base::BindOnce(&DidGetUserDataForAllRegistrations, std::move(callback)));
}

void ServiceWorkerStorageControlImpl::
    ClearUserDataForAllRegistrationsByKeyPrefix(
        const std::string& key_prefix,
        ClearUserDataForAllRegistrationsByKeyPrefixCallback callback) {
  storage_->ClearUserDataForAllRegistrationsByKeyPrefix(key_prefix,
                                                        std::move(callback));
}

void ServiceWorkerStorageControlImpl::ApplyPolicyUpdates(
    const std::vector<storage::mojom::LocalStoragePolicyUpdatePtr>
        policy_updates) {
  storage_->ApplyPolicyUpdates(std::move(policy_updates));
}

}  // namespace content
