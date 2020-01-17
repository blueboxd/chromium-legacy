// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_SERVICE_WORKER_SERVICE_WORKER_REGISTRY_H_
#define CONTENT_BROWSER_SERVICE_WORKER_SERVICE_WORKER_REGISTRY_H_

#include <memory>

#include "base/files/file_path.h"
#include "base/memory/scoped_refptr.h"
#include "content/common/content_export.h"

namespace base {
class SequencedTaskRunner;
}

namespace storage {
class QuotaManagerProxy;
class SpecialStoragePolicy;
}  // namespace storage

namespace content {

class ServiceWorkerContextCore;
class ServiceWorkerStorage;

// This class manages in-memory representation of service worker registrations
// (i.e., ServiceWorkerRegistration) including installing and uninstalling
// registrations. The instance of this class is owned by
// ServiceWorkerContextCore and has the same lifetime of the owner.
// The instance owns ServiceworkerStorage and uses it to store/retrieve
// registrations to/from persistent storage.
// TODO(crbug.com/1039200): Move ServiceWorkerStorage's method and fields which
// depend on ServiceWorkerRegistration into this class.
class CONTENT_EXPORT ServiceWorkerRegistry {
 public:
  ~ServiceWorkerRegistry();

  static std::unique_ptr<ServiceWorkerRegistry> Create(
      const base::FilePath& user_data_directory,
      ServiceWorkerContextCore* context,
      scoped_refptr<base::SequencedTaskRunner> database_task_runner,
      storage::QuotaManagerProxy* quota_manager_proxy,
      storage::SpecialStoragePolicy* special_storage_policy);

  // Re-create the registry from the old one. This is called when something went
  // wrong during storage access.
  static std::unique_ptr<ServiceWorkerRegistry> CreateForDeleteAndStartOver(
      ServiceWorkerContextCore* context,
      ServiceWorkerRegistry* old_registry);

  ServiceWorkerStorage* storage() const { return storage_.get(); }

 private:
  explicit ServiceWorkerRegistry(std::unique_ptr<ServiceWorkerStorage> storage);

  std::unique_ptr<ServiceWorkerStorage> storage_;
};

}  // namespace content

#endif  // CONTENT_BROWSER_SERVICE_WORKER_SERVICE_WORKER_REGISTRY_H_
