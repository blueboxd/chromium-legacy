// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/task_manager/providers/worker_task_provider.h"

#include "chrome/browser/browser_process.h"
#include "chrome/browser/task_manager/providers/per_profile_worker_task_tracker.h"
#include "content/public/browser/browser_thread.h"

namespace task_manager {

WorkerTaskProvider::WorkerTaskProvider() = default;

WorkerTaskProvider::~WorkerTaskProvider() = default;

Task* WorkerTaskProvider::GetTaskOfUrlRequest(int child_id, int route_id) {
  return nullptr;
}

void WorkerTaskProvider::OnProfileAdded(Profile* profile) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  observed_profiles_.Add(profile);

  auto per_profile_worker_task_tracker =
      std::make_unique<PerProfileWorkerTaskTracker>(this, profile);
  const bool inserted =
      per_profile_worker_task_trackers_
          .emplace(profile, std::move(per_profile_worker_task_tracker))
          .second;
  DCHECK(inserted);
}

void WorkerTaskProvider::OnOffTheRecordProfileCreated(Profile* off_the_record) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  OnProfileAdded(off_the_record);
}

void WorkerTaskProvider::OnProfileWillBeDestroyed(Profile* profile) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  observed_profiles_.Remove(profile);

  auto it = per_profile_worker_task_trackers_.find(profile);
  DCHECK(it != per_profile_worker_task_trackers_.end());
  per_profile_worker_task_trackers_.erase(it);
}

void WorkerTaskProvider::OnWorkerTaskAdded(Task* worker_task) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  NotifyObserverTaskAdded(worker_task);
}

void WorkerTaskProvider::OnWorkerTaskRemoved(Task* worker_task) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  // Do not forward the notification when StopUpdating() has been called, as the
  // observer is now null.
  if (!IsUpdating())
    return;

  NotifyObserverTaskRemoved(worker_task);
}

void WorkerTaskProvider::StartUpdating() {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  ProfileManager* profile_manager = g_browser_process->profile_manager();
  if (profile_manager) {
    scoped_profile_manager_observer_.Add(profile_manager);

    auto loaded_profiles = profile_manager->GetLoadedProfiles();
    for (auto* profile : loaded_profiles) {
      OnProfileAdded(profile);

      // If the incognito window is open, we have to check its profile and
      // create the tasks if there are any.
      if (profile->HasOffTheRecordProfile())
        OnProfileAdded(profile->GetOffTheRecordProfile());
    }
  }
}

void WorkerTaskProvider::StopUpdating() {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  // Stop observing profile creation and destruction.
  scoped_profile_manager_observer_.RemoveAll();
  observed_profiles_.RemoveAll();

  // Clear all ProfileWorkerTaskProvider instances to remove existing tasks.
  per_profile_worker_task_trackers_.clear();
}

}  // namespace task_manager
