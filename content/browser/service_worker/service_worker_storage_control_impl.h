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
  void StoreRegistration(
      storage::mojom::ServiceWorkerRegistrationDataPtr registration,
      std::vector<storage::mojom::ServiceWorkerResourceRecordPtr> resources,
      StoreRegistrationCallback callback) override;
  void DeleteRegistration(int64_t registration_id,
                          const GURL& origin,
                          DeleteRegistrationCallback callback) override;
  void GetNewResourceId(GetNewResourceIdCallback callback) override;
  void CreateResourceWriter(
      int64_t resource_id,
      mojo::PendingReceiver<storage::mojom::ServiceWorkerResourceWriter> writer)
      override;

  const std::unique_ptr<ServiceWorkerStorage> storage_;
};

}  // namespace content

#endif  // CONTENT_BROWSER_SERVICE_WORKER_SERVICE_WORKER_STORAGE_CONTROLIMPL_H_
