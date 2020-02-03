// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_TASK_MANAGER_PROVIDERS_PER_PROFILE_WORKER_TASK_TRACKER_H_
#define CHROME_BROWSER_TASK_MANAGER_PROVIDERS_PER_PROFILE_WORKER_TASK_TRACKER_H_

#include <memory>

#include "base/containers/flat_map.h"
#include "base/scoped_observer.h"
#include "chrome/browser/task_manager/providers/task.h"
#include "content/public/browser/service_worker_context.h"
#include "content/public/browser/service_worker_context_observer.h"

class Profile;

namespace task_manager {

class WorkerTask;
class WorkerTaskProvider;

// This is a helper class owned by WorkerTaskProvider to track all workers
// associated with a single profile. It manages the WorkerTasks and sends
// lifetime notifications to the WorkerTaskProvider.
//
// TODO(https://crbug.com/1041093): Add support for dedicated and shared
//                                  workers.
class PerProfileWorkerTaskTracker
    : public content::ServiceWorkerContextObserver {
 public:
  PerProfileWorkerTaskTracker(WorkerTaskProvider* worker_task_provider,
                              Profile* profile);

  ~PerProfileWorkerTaskTracker() override;

  PerProfileWorkerTaskTracker(const PerProfileWorkerTaskTracker&) = delete;
  PerProfileWorkerTaskTracker& operator=(const PerProfileWorkerTaskTracker&) =
      delete;

  // content::ServiceWorkerContextObserver:
  void OnVersionStartedRunning(
      int64_t version_id,
      const content::ServiceWorkerRunningInfo& running_info) override;
  void OnVersionStoppedRunning(int64_t version_id) override;

 private:
  // Creates a service worker task and inserts it into |service_worker_tasks_|.
  // Then it notifies the |worker_task_provider_| about the new Task.
  void CreateWorkerTask(int version_id,
                        int worker_process_id,
                        const GURL& script_url);

  // Deletes an existing service worker task from |service_worker_tasks_| and
  // notifies |worker_task_provider_| about the deletion of the task.
  void DeleteWorkerTask(int version_id);

  // The provider that gets notified when a WorkerTask is created/deleted.
  WorkerTaskProvider* const worker_task_provider_;  // Owner.

  ScopedObserver<content::ServiceWorkerContext,
                 content::ServiceWorkerContextObserver>
      scoped_service_worker_context_observer_{this};

  base::flat_map<int64_t /*version_id*/, std::unique_ptr<WorkerTask>>
      service_worker_tasks_;
};

}  // namespace task_manager

#endif  // CHROME_BROWSER_TASK_MANAGER_PROVIDERS_PER_PROFILE_WORKER_TASK_TRACKER_H_
