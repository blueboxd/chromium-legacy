// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/web_applications/components/manifest_update_manager.h"

#include "chrome/browser/web_applications/components/web_app_constants.h"
#include "chrome/common/chrome_features.h"

namespace web_app {

ManifestUpdateManager::ManifestUpdateManager() = default;

ManifestUpdateManager::~ManifestUpdateManager() = default;

void ManifestUpdateManager::SetSubsystems(AppRegistrar* registrar,
                                          WebAppUiManager* ui_manager,
                                          InstallManager* install_manager) {
  registrar_ = registrar;
  ui_manager_ = ui_manager;
  install_manager_ = install_manager;
}

void ManifestUpdateManager::Start() {
  registrar_observer_.Add(registrar_);
}

void ManifestUpdateManager::Shutdown() {
  registrar_observer_.RemoveAll();
}

void ManifestUpdateManager::MaybeUpdate(const GURL& url,
                                        const AppId& app_id,
                                        content::WebContents* web_contents) {
  if (!base::FeatureList::IsEnabled(features::kDesktopPWAsLocalUpdating))
    return;

  if (app_id.empty() || !registrar_->IsLocallyInstalled(app_id)) {
    NotifyResult(url, ManifestUpdateResult::kNoAppInScope);
    return;
  }

  std::unique_ptr<ManifestUpdateTask>& current_task = tasks_[app_id];
  if (current_task)
    return;

  if (!MaybeConsumeUpdateCheck(app_id)) {
    NotifyResult(url, ManifestUpdateResult::kThrottled);
    return;
  }

  current_task = std::make_unique<ManifestUpdateTask>(
      url, app_id, web_contents,
      base::Bind(&ManifestUpdateManager::OnUpdateStopped,
                 base::Unretained(this)),
      hang_update_checks_for_testing_, *registrar_, ui_manager_,
      install_manager_);
}

// AppRegistrarObserver:
void ManifestUpdateManager::OnWebAppUninstalled(const AppId& app_id) {
  auto it = tasks_.find(app_id);
  if (it != tasks_.end()) {
    NotifyResult(it->second->url(), ManifestUpdateResult::kAppUninstalled);
    tasks_.erase(it);
  }
  DCHECK(!tasks_.contains(app_id));
}

bool ManifestUpdateManager::MaybeConsumeUpdateCheck(const AppId& app_id) {
  base::Time now = time_override_for_testing_.value_or(base::Time::Now());
  base::Time& last_check_time = last_check_times_[app_id];

  // Throttling updates to at most once per day is consistent with Android.
  // See |UPDATE_INTERVAL| in WebappDataStorage.java.
  constexpr base::TimeDelta kDelayBetweenChecks = base::TimeDelta::FromDays(1);
  if (now < last_check_time + kDelayBetweenChecks)
    return false;

  last_check_time = now;
  return true;
}

void ManifestUpdateManager::OnUpdateStopped(const ManifestUpdateTask& task,
                                            ManifestUpdateResult result) {
  DCHECK_EQ(&task, tasks_[task.app_id()].get());
  NotifyResult(task.url(), result);
  tasks_.erase(task.app_id());
}

void ManifestUpdateManager::SetResultCallbackForTesting(
    ResultCallback callback) {
  DCHECK(result_callback_for_testing_.is_null());
  result_callback_for_testing_ = std::move(callback);
}

void ManifestUpdateManager::NotifyResult(const GURL& url,
                                         ManifestUpdateResult result) {
  if (result_callback_for_testing_)
    std::move(result_callback_for_testing_).Run(url, result);
}

}  // namespace web_app
