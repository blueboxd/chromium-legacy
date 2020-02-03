// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/task_manager/providers/per_profile_worker_task_tracker.h"

#include "base/logging.h"
#include "chrome/browser/task_manager/providers/worker_task.h"
#include "chrome/browser/task_manager/providers/worker_task_provider.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/storage_partition.h"

namespace task_manager {

PerProfileWorkerTaskTracker::PerProfileWorkerTaskTracker(
    WorkerTaskProvider* worker_task_provider,
    Profile* profile)
    : worker_task_provider_(worker_task_provider) {
  DCHECK(worker_task_provider);
  DCHECK(profile);

  content::StoragePartition* storage_partition =
      content::BrowserContext::GetDefaultStoragePartition(profile);

  content::ServiceWorkerContext* service_worker_context =
      storage_partition->GetServiceWorkerContext();
  scoped_service_worker_context_observer_.Add(service_worker_context);

  for (const auto& kv :
       service_worker_context->GetRunningServiceWorkerInfos()) {
    const int64_t version_id = kv.first;
    const content::ServiceWorkerRunningInfo& running_info = kv.second;
    OnVersionStartedRunning(version_id, running_info);
  }
}

PerProfileWorkerTaskTracker::~PerProfileWorkerTaskTracker() = default;

void PerProfileWorkerTaskTracker::OnVersionStartedRunning(
    int64_t version_id,
    const content::ServiceWorkerRunningInfo& running_info) {
  CreateWorkerTask(version_id, running_info.render_process_id,
                   running_info.script_url);
}

void PerProfileWorkerTaskTracker::OnVersionStoppedRunning(int64_t version_id) {
  DeleteWorkerTask(version_id);
}

void PerProfileWorkerTaskTracker::CreateWorkerTask(int version_id,
                                                   int worker_process_id,
                                                   const GURL& script_url) {
  auto* worker_process_host =
      content::RenderProcessHost::FromID(worker_process_id);
  auto insertion_result = service_worker_tasks_.emplace(
      version_id, std::make_unique<WorkerTask>(
                      worker_process_host->GetProcess().Handle(), script_url,
                      Task::Type::SERVICE_WORKER, worker_process_id));
  DCHECK(insertion_result.second);
  worker_task_provider_->OnWorkerTaskAdded(
      insertion_result.first->second.get());
}

void PerProfileWorkerTaskTracker::DeleteWorkerTask(int version_id) {
  auto it = service_worker_tasks_.find(version_id);
  DCHECK(it != service_worker_tasks_.end());
  worker_task_provider_->OnWorkerTaskRemoved(it->second.get());
  service_worker_tasks_.erase(it);
}

}  // namespace task_manager
