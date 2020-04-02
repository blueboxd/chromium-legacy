// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/service_worker/service_worker_storage_control_impl.h"

#include "content/browser/service_worker/service_worker_resource_writer_impl.h"
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

void ServiceWorkerStorageControlImpl::GetNewResourceId(
    GetNewResourceIdCallback callback) {
  storage_->GetNewResourceId(std::move(callback));
}

void ServiceWorkerStorageControlImpl::CreateResourceWriter(
    int64_t resource_id,
    mojo::PendingReceiver<storage::mojom::ServiceWorkerResourceWriter> writer) {
  DCHECK_NE(resource_id, blink::mojom::kInvalidServiceWorkerResourceId);
  mojo::MakeSelfOwnedReceiver(std::make_unique<ServiceWorkerResourceWriterImpl>(
                                  storage_->CreateResponseWriter(resource_id)),
                              std::move(writer));
}

}  // namespace content
