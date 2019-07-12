// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/task_manager/providers/service_worker_task_provider.h"

#include "base/stl_util.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/chrome_notification_types.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/notification_service.h"
#include "content/public/browser/notification_source.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/service_worker_context.h"
#include "content/public/browser/service_worker_running_info.h"
#include "content/public/browser/storage_partition.h"
#include "content/public/common/child_process_host.h"
#include "third_party/blink/public/mojom/service_worker/service_worker_object.mojom.h"

using content::BrowserThread;

namespace task_manager {

ServiceWorkerTaskProvider::ServiceWorkerTaskProvider() {}

ServiceWorkerTaskProvider::~ServiceWorkerTaskProvider() = default;

Task* ServiceWorkerTaskProvider::GetTaskOfUrlRequest(int child_id,
                                                     int route_id) {
  return nullptr;
}

void ServiceWorkerTaskProvider::Observe(
    int type,
    const content::NotificationSource& source,
    const content::NotificationDetails& details) {
  switch (type) {
    case chrome::NOTIFICATION_PROFILE_CREATED: {
      OnProfileCreated(content::Source<Profile>(source).ptr());
      break;
    }
    default:
      NOTREACHED() << type;
  }
}

void ServiceWorkerTaskProvider::OnVersionRunningStatusChanged(
    content::ServiceWorkerContext* context,
    int64_t version_id,
    bool is_running) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  const ServiceWorkerTaskKey key(context, version_id);

#if DCHECK_IS_ON()
  if (is_running) {
    DCHECK(!base::Contains(service_worker_task_map_, key) &&
           !base::Contains(tasks_to_be_created_, key));
  } else {
    DCHECK(base::Contains(service_worker_task_map_, key) ||
           base::Contains(tasks_to_be_created_, key));
  }
#endif  // DCHECK_IS_ON()

  if (is_running) {
    tasks_to_be_created_.emplace(key);

    // Create a new task since the service worker is running now.
    context->GetServiceWorkerRunningInfo(
        version_id,
        base::BindOnce(
            &ServiceWorkerTaskProvider::OnDidGetServiceWorkerRunningInfo,
            weak_ptr_factory_.GetWeakPtr()));
  } else {
    auto iter = tasks_to_be_created_.find(key);

    // If the task is not created yet, erase it from |tasks_to_be_created_|.
    if (iter != tasks_to_be_created_.end()) {
      tasks_to_be_created_.erase(iter);
      return;
    }

    // Since the task has been created and the service worker is not running
    // any longer, delete it.
    DeleteTask(context, version_id);
  }
}

void ServiceWorkerTaskProvider::OnDestruct(
    content::ServiceWorkerContext* context) {
  auto found = observed_contexts_.find(context);
  if (found == observed_contexts_.end())
    return;

  (*found)->RemoveObserver(this);
  observed_contexts_.erase(found);
  DeleteAllTasks(context);
}

void ServiceWorkerTaskProvider::StartUpdating() {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  registrar_.Add(this, chrome::NOTIFICATION_PROFILE_CREATED,
                 content::NotificationService::AllBrowserContextsAndSources());

  ProfileManager* profile_manager = g_browser_process->profile_manager();
  if (profile_manager) {
    auto loaded_profiles = profile_manager->GetLoadedProfiles();
    for (auto* profile : loaded_profiles) {
      CreateTasksForProfile(profile);

      // If the incognito window is open, we have to check its profile and
      // create the tasks if there are any.
      if (profile->HasOffTheRecordProfile())
        CreateTasksForProfile(profile->GetOffTheRecordProfile());
    }
  }
}

void ServiceWorkerTaskProvider::StopUpdating() {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  // OnDidGetAllServiceWorkerRunningInfos()/OnDidGetServiceWorkerRunningInfo()
  // should never be called after this, and hence we must invalidate the weak
  // pointers.
  weak_ptr_factory_.InvalidateWeakPtrs();

  // Stop listening to NOTIFICATION_PROFILE_CREATED.
  registrar_.RemoveAll();

  // Stop observing.
  for (auto* context : observed_contexts_)
    context->RemoveObserver(this);
  observed_contexts_.clear();

  // Delete all tasks that will be created.
  tasks_to_be_created_.clear();

  // Delete all tracked tasks.
  service_worker_task_map_.clear();
}

void ServiceWorkerTaskProvider::CreateTasksForProfile(Profile* profile) {
  content::ServiceWorkerContext* context =
      content::BrowserContext::GetDefaultStoragePartition(profile)
          ->GetServiceWorkerContext();
  context->GetAllServiceWorkerRunningInfos(base::BindOnce(
      &ServiceWorkerTaskProvider::OnDidGetAllServiceWorkerRunningInfos,
      weak_ptr_factory_.GetWeakPtr()));
}

void ServiceWorkerTaskProvider::CreateTask(
    content::ServiceWorkerContext* context,
    const content::ServiceWorkerRunningInfo& service_worker_running_info) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  // The version may have been running, but by the time we got its running info
  // from the IO thread it may have stopped.
  if (service_worker_running_info.process_id ==
          content::ChildProcessHost::kInvalidUniqueID ||
      service_worker_running_info.version_id ==
          blink::mojom::kInvalidServiceWorkerVersionId) {
    return;
  }

  const ServiceWorkerTaskKey key(context,
                                 service_worker_running_info.version_id);
  DCHECK(!base::Contains(service_worker_task_map_, key));

  auto* host = content::RenderProcessHost::FromID(
      service_worker_running_info.process_id);
  auto result = service_worker_task_map_.emplace(
      key, std::make_unique<ServiceWorkerTask>(service_worker_running_info,
                                               host->GetProcess().Handle()));

  DCHECK(result.second);
  NotifyObserverTaskAdded(result.first->second.get());
}

void ServiceWorkerTaskProvider::DeleteTask(
    content::ServiceWorkerContext* context,
    int version_id) {
  const ServiceWorkerTaskKey key(context, version_id);
  auto it = service_worker_task_map_.find(key);

  if (it == service_worker_task_map_.end())
    return;

  NotifyObserverTaskRemoved(it->second.get());
  service_worker_task_map_.erase(it);
}

void ServiceWorkerTaskProvider::DeleteAllTasks(
    content::ServiceWorkerContext* context) {
  for (auto it = service_worker_task_map_.begin();
       it != service_worker_task_map_.end();) {
    if (it->first.first == context) {
      NotifyObserverTaskRemoved(it->second.get());
      it = service_worker_task_map_.erase(it);
    } else {
      ++it;
    }
  }
}

void ServiceWorkerTaskProvider::OnDidGetAllServiceWorkerRunningInfos(
    content::ServiceWorkerContext* context,
    std::vector<content::ServiceWorkerRunningInfo> running_infos) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  for (const auto& running_info : running_infos)
    CreateTask(context, running_info);

  if (observed_contexts_.find(context) == observed_contexts_.end()) {
    // Start observing.
    context->AddObserver(this);
    observed_contexts_.emplace(context);
  }
}

void ServiceWorkerTaskProvider::OnDidGetServiceWorkerRunningInfo(
    content::ServiceWorkerContext* context,
    const content::ServiceWorkerRunningInfo& running_info) {
  auto key = std::make_pair(context, running_info.version_id);

  // Don't create a task if the version was stopped and the entry was erased
  // while getting the info.
  if (tasks_to_be_created_.find(key) == tasks_to_be_created_.end())
    return;

  CreateTask(context, running_info);
  tasks_to_be_created_.erase(key);
}

void ServiceWorkerTaskProvider::OnProfileCreated(Profile* profile) {
  content::ServiceWorkerContext* context =
      content::BrowserContext::GetDefaultStoragePartition(profile)
          ->GetServiceWorkerContext();
  if (observed_contexts_.find(context) == observed_contexts_.end()) {
    context->AddObserver(this);
    observed_contexts_.emplace(context);
  }
}

}  // namespace task_manager
