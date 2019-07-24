// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/background_sync/background_sync_proxy.h"

#include <utility>

#include "base/bind.h"
#include "base/location.h"
#include "base/macros.h"
#include "base/task/post_task.h"
#include "base/task/task_traits.h"
#include "content/browser/service_worker/service_worker_context_wrapper.h"
#include "content/browser/storage_partition_impl.h"
#include "content/public/browser/background_sync_context.h"
#include "content/public/browser/background_sync_controller.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/browser_task_traits.h"

namespace content {

class BackgroundSyncProxy::Core {
 public:
  Core(const base::WeakPtr<BackgroundSyncProxy>& io_parent,
       scoped_refptr<ServiceWorkerContextWrapper> service_worker_context)
      : io_parent_(io_parent),
        service_worker_context_(std::move(service_worker_context)),
        weak_ptr_factory_(this) {
    DCHECK_CURRENTLY_ON(BrowserThread::IO);
    DCHECK(service_worker_context_);
  }

  ~Core() { DCHECK_CURRENTLY_ON(BrowserThread::UI); }

  base::WeakPtr<Core> GetWeakPtrOnIO() {
    DCHECK_CURRENTLY_ON(BrowserThread::IO);
    return weak_ptr_factory_.GetWeakPtr();
  }

  BrowserContext* browser_context() {
    DCHECK_CURRENTLY_ON(BrowserThread::UI);
    if (!service_worker_context_)
      return nullptr;

    StoragePartitionImpl* storage_partition_impl =
        service_worker_context_->storage_partition();
    if (!storage_partition_impl)  // may be null in tests
      return nullptr;

    return storage_partition_impl->browser_context();
  }

  // TODO(crbug.com/982378): Schedule a task to periodically revive suspended
  // periodic Background Sync registrations.
  void ScheduleBrowserWakeUp(blink::mojom::BackgroundSyncType sync_type) {
    DCHECK_CURRENTLY_ON(BrowserThread::UI);

    if (!browser_context())
      return;

    auto* controller = browser_context()->GetBackgroundSyncController();
    DCHECK(controller);

    controller->ScheduleBrowserWakeUp(sync_type);
  }

  void SendSuspendedPeriodicSyncOrigins(
      std::set<url::Origin> suspended_origins) {
    DCHECK_CURRENTLY_ON(BrowserThread::UI);
    if (!browser_context())
      return;

    auto* controller = browser_context()->GetBackgroundSyncController();
    DCHECK(controller);

    controller->NoteSuspendedPeriodicSyncOrigins(std::move(suspended_origins));
  }

 private:
  base::WeakPtr<BackgroundSyncProxy> io_parent_;
  scoped_refptr<ServiceWorkerContextWrapper> service_worker_context_;
  base::WeakPtrFactory<Core> weak_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN(Core);
};

BackgroundSyncProxy::BackgroundSyncProxy(
    scoped_refptr<ServiceWorkerContextWrapper> service_worker_context)
    : weak_ptr_factory_(this) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  DCHECK(service_worker_context);
  ui_core_ = std::unique_ptr<Core, BrowserThread::DeleteOnUIThread>(new Core(
      weak_ptr_factory_.GetWeakPtr(), std::move(service_worker_context)));
  ui_core_weak_ptr_ = ui_core_->GetWeakPtrOnIO();
}

BackgroundSyncProxy::~BackgroundSyncProxy() = default;

void BackgroundSyncProxy::ScheduleBrowserWakeUp(
    blink::mojom::BackgroundSyncType sync_type) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);

  // Schedule Chrome wakeup.
  base::PostTaskWithTraits(FROM_HERE, {BrowserThread::UI},
                           base::BindOnce(&Core::ScheduleBrowserWakeUp,
                                          ui_core_weak_ptr_, sync_type));
}

void BackgroundSyncProxy::SendSuspendedPeriodicSyncOrigins(
    std::set<url::Origin> suspended_origins) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);

  base::PostTaskWithTraits(
      FROM_HERE, {BrowserThread::UI},
      base::BindOnce(&Core::SendSuspendedPeriodicSyncOrigins, ui_core_weak_ptr_,
                     std::move(suspended_origins)));
}

}  // namespace content
