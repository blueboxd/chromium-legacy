// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_TASK_MANAGER_PROVIDERS_SERVICE_WORKER_TASK_PROVIDER_H_
#define CHROME_BROWSER_TASK_MANAGER_PROVIDERS_SERVICE_WORKER_TASK_PROVIDER_H_

#include <map>
#include <memory>
#include <utility>
#include <vector>

#include "base/containers/flat_set.h"
#include "base/memory/weak_ptr.h"
#include "chrome/browser/task_manager/providers/service_worker_task.h"
#include "chrome/browser/task_manager/providers/task_provider.h"
#include "content/public/browser/notification_observer.h"
#include "content/public/browser/notification_registrar.h"
#include "content/public/browser/service_worker_context_observer.h"

namespace content {
class ServiceWorkerContext;
}

namespace task_manager {

// This provides tasks that describe running service workers
// (https://w3c.github.io/ServiceWorker/). It adds itself as an observer of
// ServiceWorkerContext to listen to the running status changes of the service
// workers for the creation and destruction of the tasks.
class ServiceWorkerTaskProvider : public TaskProvider,
                                  public content::NotificationObserver,
                                  public content::ServiceWorkerContextObserver {
 public:
  ServiceWorkerTaskProvider();
  ~ServiceWorkerTaskProvider() override;

  // task_manager::TaskProvider:
  Task* GetTaskOfUrlRequest(int child_id, int route_id) override;

  // content::NotificationObserver:
  void Observe(int type,
               const content::NotificationSource& source,
               const content::NotificationDetails& details) override;

  // content::ServiceWorkerContextObserver:
  void OnVersionRunningStatusChanged(content::ServiceWorkerContext* context,
                                     int64_t version_id,
                                     bool is_running) override;
  void OnDestruct(content::ServiceWorkerContext* context) override;

 private:
  // task_manager::TaskProvider:
  void StartUpdating() override;
  void StopUpdating() override;

  // Creates ServiceWorkerTasks for the given |profile| and notifies the
  // observer of their additions.
  void CreateTasksForProfile(Profile* profile);

  // Creates a ServiceWorkerTask from the given |running_info| and notifies the
  // observer of its addition.
  void CreateTask(content::ServiceWorkerContext* context,
                  const content::ServiceWorkerRunningInfo& running_info);

  // Deletes a ServiceWorkerTask with the |version_id| after notifying the
  // observer of its deletion.
  void DeleteTask(content::ServiceWorkerContext* context, int version_id);

  // Deletes all ServiceWorkerTasks which relate to |context| after notifying
  // the observer of their deletions.
  void DeleteAllTasks(content::ServiceWorkerContext* context);

  // Called back on the UI thread when the infos of multiple existing running
  // service workers in |context| have been retrieved on the IO thread.
  void OnDidGetAllServiceWorkerRunningInfos(
      content::ServiceWorkerContext* context,
      std::vector<content::ServiceWorkerRunningInfo> running_infos);

  // Called back on the UI thread when the info of a single existing running
  // service worker in |context| has been retrieved on the IO thread.
  void OnDidGetServiceWorkerRunningInfo(
      content::ServiceWorkerContext* context,
      const content::ServiceWorkerRunningInfo& running_info);

  // Called after a profile has been created.
  void OnProfileCreated(Profile* profile);

  using ServiceWorkerTaskKey =
      std::pair<content::ServiceWorkerContext*, int64_t /*version_id*/>;
  using ServiceWorkerTaskMap =
      std::map<ServiceWorkerTaskKey, std::unique_ptr<ServiceWorkerTask>>;
  ServiceWorkerTaskMap service_worker_task_map_;

  base::flat_set<
      std::pair<content::ServiceWorkerContext*, int64_t /*version_id*/>>
      tasks_to_be_created_;

  base::flat_set<content::ServiceWorkerContext*> observed_contexts_;

  content::NotificationRegistrar registrar_;

  base::WeakPtrFactory<ServiceWorkerTaskProvider> weak_ptr_factory_{this};
};

}  // namespace task_manager

#endif  // CHROME_BROWSER_TASK_MANAGER_PROVIDERS_SERVICE_WORKER_TASK_PROVIDER_H_
