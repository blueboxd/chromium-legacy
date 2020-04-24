// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/task_manager/providers/per_profile_worker_task_tracker.h"

#include "base/check.h"
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

  // Dedicated workers:
  content::DedicatedWorkerService* dedicated_worker_service =
      storage_partition->GetDedicatedWorkerService();
  scoped_dedicated_worker_service_observer_.Add(dedicated_worker_service);
  dedicated_worker_service->EnumerateDedicatedWorkers(this);

  // Shared workers:
  content::SharedWorkerService* shared_worker_service =
      storage_partition->GetSharedWorkerService();
  scoped_shared_worker_service_observer_.Add(shared_worker_service);
  shared_worker_service->EnumerateSharedWorkers(this);

  // Service workers:
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

PerProfileWorkerTaskTracker::~PerProfileWorkerTaskTracker() {
  // Notify the |worker_task_provider_| for all outstanding tasks that are about
  // to be deleted.
  for (const auto& kv : dedicated_worker_tasks_)
    worker_task_provider_->OnWorkerTaskRemoved(kv.second.get());
  for (const auto& kv : shared_worker_tasks_)
    worker_task_provider_->OnWorkerTaskRemoved(kv.second.get());
  for (const auto& kv : service_worker_tasks_)
    worker_task_provider_->OnWorkerTaskRemoved(kv.second.get());
}

void PerProfileWorkerTaskTracker::OnWorkerStarted(
    content::DedicatedWorkerId dedicated_worker_id,
    int worker_process_id,
    content::GlobalFrameRoutingId ancestor_render_frame_host_id) {
  CreateWorkerTask(dedicated_worker_id, Task::Type::DEDICATED_WORKER,
                   worker_process_id, &dedicated_worker_tasks_);
}

void PerProfileWorkerTaskTracker::OnBeforeWorkerTerminated(
    content::DedicatedWorkerId dedicated_worker_id,
    content::GlobalFrameRoutingId ancestor_render_frame_host_id) {
  DeleteWorkerTask(dedicated_worker_id, &dedicated_worker_tasks_);
}

void PerProfileWorkerTaskTracker::OnFinalResponseURLDetermined(
    content::DedicatedWorkerId dedicated_worker_id,
    const GURL& url) {
  SetWorkerTaskScriptUrl(dedicated_worker_id, url, &dedicated_worker_tasks_);
}

void PerProfileWorkerTaskTracker::OnWorkerStarted(
    content::SharedWorkerId shared_worker_id,
    int worker_process_id,
    const base::UnguessableToken& dev_tools_token) {
  CreateWorkerTask(shared_worker_id, Task::Type::SHARED_WORKER,
                   worker_process_id, &shared_worker_tasks_);
}

void PerProfileWorkerTaskTracker::OnBeforeWorkerTerminated(
    content::SharedWorkerId shared_worker_id) {
  DeleteWorkerTask(shared_worker_id, &shared_worker_tasks_);
}

void PerProfileWorkerTaskTracker::OnFinalResponseURLDetermined(
    content::SharedWorkerId shared_worker_id,
    const GURL& url) {
  SetWorkerTaskScriptUrl(shared_worker_id, url, &shared_worker_tasks_);
}

void PerProfileWorkerTaskTracker::OnVersionStartedRunning(
    int64_t version_id,
    const content::ServiceWorkerRunningInfo& running_info) {
  CreateWorkerTask(version_id, Task::Type::SERVICE_WORKER,
                   running_info.render_process_id, &service_worker_tasks_);
  SetWorkerTaskScriptUrl(version_id, running_info.script_url,
                         &service_worker_tasks_);
}

void PerProfileWorkerTaskTracker::OnVersionStoppedRunning(int64_t version_id) {
  DeleteWorkerTask(version_id, &service_worker_tasks_);
}

template <typename WorkerId>
void PerProfileWorkerTaskTracker::CreateWorkerTask(
    const WorkerId& worker_id,
    Task::Type task_type,
    int worker_process_id,
    base::flat_map<WorkerId, std::unique_ptr<WorkerTask>>* out_worker_tasks) {
  auto* worker_process_host =
      content::RenderProcessHost::FromID(worker_process_id);
  auto insertion_result = out_worker_tasks->emplace(
      worker_id,
      std::make_unique<WorkerTask>(worker_process_host->GetProcess().Handle(),
                                   task_type, worker_process_id));
  DCHECK(insertion_result.second);
  worker_task_provider_->OnWorkerTaskAdded(
      insertion_result.first->second.get());
}

template <typename WorkerId>
void PerProfileWorkerTaskTracker::DeleteWorkerTask(
    const WorkerId& worker_id,
    base::flat_map<WorkerId, std::unique_ptr<WorkerTask>>* out_worker_tasks) {
  auto it = out_worker_tasks->find(worker_id);
  DCHECK(it != out_worker_tasks->end());
  worker_task_provider_->OnWorkerTaskRemoved(it->second.get());
  out_worker_tasks->erase(it);
}

template <typename WorkerId>
void PerProfileWorkerTaskTracker::SetWorkerTaskScriptUrl(
    const WorkerId& worker_id,
    const GURL& script_url,
    base::flat_map<WorkerId, std::unique_ptr<WorkerTask>>* out_worker_tasks) {
  auto it = out_worker_tasks->find(worker_id);
  DCHECK(it != out_worker_tasks->end());
  it->second->SetScriptUrl(script_url);
}

}  // namespace task_manager
