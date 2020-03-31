// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/service_worker/service_worker_storage_control_impl.h"

#include "content/browser/service_worker/service_worker_storage.h"

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

}  // namespace

ServiceWorkerStorageControlImpl::ServiceWorkerStorageControlImpl(
    std::unique_ptr<ServiceWorkerStorage> storage)
    : storage_(std::move(storage)) {
  DCHECK(storage_);
}

ServiceWorkerStorageControlImpl::~ServiceWorkerStorageControlImpl() = default;

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

}  // namespace content
