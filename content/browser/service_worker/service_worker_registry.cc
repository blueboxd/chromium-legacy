// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/service_worker/service_worker_registry.h"

#include "base/memory/ptr_util.h"
#include "content/browser/service_worker/service_worker_storage.h"

namespace content {

// static
std::unique_ptr<ServiceWorkerRegistry> ServiceWorkerRegistry::Create(
    const base::FilePath& user_data_directory,
    ServiceWorkerContextCore* context,
    scoped_refptr<base::SequencedTaskRunner> database_task_runner,
    storage::QuotaManagerProxy* quota_manager_proxy,
    storage::SpecialStoragePolicy* special_storage_policy) {
  DCHECK(context);
  auto storage = ServiceWorkerStorage::Create(
      user_data_directory, context, std::move(database_task_runner),
      quota_manager_proxy, special_storage_policy);
  return base::WrapUnique(new ServiceWorkerRegistry(std::move(storage)));
}

// static
std::unique_ptr<ServiceWorkerRegistry>
ServiceWorkerRegistry::CreateForDeleteAndStartOver(
    ServiceWorkerContextCore* context,
    ServiceWorkerRegistry* old_registry) {
  DCHECK(context);
  DCHECK(old_registry);
  auto new_storage =
      ServiceWorkerStorage::Create(context, old_registry->storage());
  return base::WrapUnique(new ServiceWorkerRegistry(std::move(new_storage)));
}

ServiceWorkerRegistry::ServiceWorkerRegistry(
    std::unique_ptr<ServiceWorkerStorage> storage)
    : storage_(std::move(storage)) {
  DCHECK(storage_);
}

ServiceWorkerRegistry::~ServiceWorkerRegistry() = default;

}  // namespace content
