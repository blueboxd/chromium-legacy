// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_TASK_MANAGER_PROVIDERS_WORKER_TASK_PROVIDER_H_
#define CHROME_BROWSER_TASK_MANAGER_PROVIDERS_WORKER_TASK_PROVIDER_H_

#include <map>
#include <memory>
#include <utility>

#include "base/scoped_observer.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/profiles/profile_manager_observer.h"
#include "chrome/browser/profiles/profile_observer.h"
#include "chrome/browser/task_manager/providers/task_provider.h"
#include "chrome/browser/task_manager/providers/worker_task.h"
#include "content/public/browser/service_worker_context.h"
#include "content/public/browser/service_worker_context_observer.h"

namespace task_manager {

// This class provides tasks that describe running workers of all types
// (dedicated, shared or service workers).
//
// See https://w3c.github.io/workers/ or https://w3c.github.io/ServiceWorker/
// for more details.
//
// TODO(https://crbug.com/1041093): Add support for dedicated workers and shared
//                                  workers.
class WorkerTaskProvider : public TaskProvider,
                           public ProfileManagerObserver,
                           public ProfileObserver,
                           public content::ServiceWorkerContextObserver {
 public:
  WorkerTaskProvider();
  ~WorkerTaskProvider() override;

  // Non-copyable.
  WorkerTaskProvider(const WorkerTaskProvider& other) = delete;
  WorkerTaskProvider& operator=(const WorkerTaskProvider& other) = delete;

  // task_manager::TaskProvider:
  Task* GetTaskOfUrlRequest(int child_id, int route_id) override;

  // ProfileManagerObserver:
  void OnProfileAdded(Profile* profile) override;

  // ProfileObserver:
  void OnOffTheRecordProfileCreated(Profile* off_the_record) override;
  void OnProfileWillBeDestroyed(Profile* profile) override;

  // content::ServiceWorkerContextObserver:
  void OnVersionStartedRunning(
      content::ServiceWorkerContext* context,
      int64_t version_id,
      const content::ServiceWorkerRunningInfo& running_info) override;
  void OnVersionStoppedRunning(content::ServiceWorkerContext* context,
                               int64_t version_id) override;

 private:
  // task_manager::TaskProvider:
  void StartUpdating() override;
  void StopUpdating() override;

  // Creates WorkerTasks for the given |profile| and notifies the observer of
  // their additions.
  void CreateTasksForProfile(Profile* profile);

  // Creates a WorkerTask from the given |running_info| and notifies the
  // observer of its addition.
  void CreateTask(content::ServiceWorkerContext* context,
                  int64_t version_id,
                  const content::ServiceWorkerRunningInfo& running_info);

  // Deletes a WorkerTask associated with |version_id| after notifying the
  // observer of its deletion.
  void DeleteTask(content::ServiceWorkerContext* context, int version_id);

  ScopedObserver<ProfileManager, ProfileManagerObserver>
      scoped_profile_manager_observer_{this};

  ScopedObserver<Profile, ProfileObserver> observed_profiles_{this};

  using ServiceWorkerTaskKey =
      std::pair<content::ServiceWorkerContext*, int64_t /*version_id*/>;
  using ServiceWorkerTaskMap =
      std::map<ServiceWorkerTaskKey, std::unique_ptr<WorkerTask>>;
  ServiceWorkerTaskMap service_worker_task_map_;

  ScopedObserver<content::ServiceWorkerContext,
                 content::ServiceWorkerContextObserver>
      scoped_context_observer_{this};
};

}  // namespace task_manager

#endif  // CHROME_BROWSER_TASK_MANAGER_PROVIDERS_WORKER_TASK_PROVIDER_H_
