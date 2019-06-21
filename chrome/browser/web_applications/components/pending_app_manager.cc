// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/web_applications/components/pending_app_manager.h"

#include <algorithm>
#include <memory>
#include <utility>

#include "base/bind_helpers.h"
#include "base/stl_util.h"
#include "base/threading/thread_task_runner_handle.h"
#include "chrome/browser/web_applications/components/app_registrar.h"
#include "chrome/browser/web_applications/components/web_app_constants.h"

namespace web_app {

PendingAppManager::SynchronizeRequest::SynchronizeRequest(
    SynchronizeCallback callback,
    int remaining_requests)
    : callback(std::move(callback)), remaining_requests(remaining_requests) {}

PendingAppManager::SynchronizeRequest::~SynchronizeRequest() = default;

PendingAppManager::SynchronizeRequest& PendingAppManager::SynchronizeRequest::
operator=(PendingAppManager::SynchronizeRequest&&) = default;

PendingAppManager::SynchronizeRequest::SynchronizeRequest(
    SynchronizeRequest&& other) = default;

PendingAppManager::PendingAppManager(AppRegistrar* registrar)
    : registrar_(registrar) {}

PendingAppManager::~PendingAppManager() = default;

void PendingAppManager::SynchronizeInstalledApps(
    std::vector<InstallOptions> desired_apps_install_options,
    InstallSource install_source,
    SynchronizeCallback callback) {
  DCHECK(registrar_);
  DCHECK(std::all_of(desired_apps_install_options.begin(),
                     desired_apps_install_options.end(),
                     [&install_source](const InstallOptions& install_options) {
                       return install_options.install_source == install_source;
                     }));
  // Only one concurrent SynchronizeInstalledApps() expected per InstallSource.
  DCHECK(!base::Contains(synchronize_requests_, install_source));

  std::vector<GURL> installed_urls;
  for (auto apps_it : registrar_->GetExternallyInstalledApps(install_source))
    installed_urls.push_back(apps_it.second);

  std::sort(installed_urls.begin(), installed_urls.end());

  std::vector<GURL> desired_urls;
  for (const auto& info : desired_apps_install_options)
    desired_urls.push_back(info.url);

  std::sort(desired_urls.begin(), desired_urls.end());

  auto urls_to_remove =
      base::STLSetDifference<std::vector<GURL>>(installed_urls, desired_urls);

  // Run callback immediately if there's no work to be done.
  if (urls_to_remove.empty() && desired_apps_install_options.empty()) {
    base::ThreadTaskRunnerHandle::Get()->PostTask(
        FROM_HERE,
        base::BindOnce(std::move(callback), SynchronizeResult::kSuccess));
    return;
  }

  // Add the callback to a map and call once all installs/uninstalls finish.
  synchronize_requests_.insert_or_assign(
      install_source,
      SynchronizeRequest(std::move(callback),
                         urls_to_remove.size() + desired_urls.size()));

  UninstallApps(
      urls_to_remove,
      base::BindRepeating(&PendingAppManager::UninstallForSynchronizeCallback,
                          weak_ptr_factory_.GetWeakPtr(), install_source));
  InstallApps(
      std::move(desired_apps_install_options),
      base::BindRepeating(&PendingAppManager::InstallForSynchronizeCallback,
                          weak_ptr_factory_.GetWeakPtr(), install_source));
}

void PendingAppManager::InstallForSynchronizeCallback(InstallSource source,
                                                      const GURL& app_url,
                                                      InstallResultCode code) {
  OnAppSynchronized(source, code == InstallResultCode::kSuccess ||
                                code == InstallResultCode::kAlreadyInstalled);
}

void PendingAppManager::UninstallForSynchronizeCallback(InstallSource source,
                                                        const GURL& app_url,
                                                        bool succeeded) {
  OnAppSynchronized(source, succeeded);
}

void PendingAppManager::OnAppSynchronized(InstallSource source,
                                          bool succeeded) {
  auto it = synchronize_requests_.find(source);
  DCHECK(it != synchronize_requests_.end());

  SynchronizeRequest& request = it->second;
  if (!succeeded)
    request.result = SynchronizeResult::kFailed;

  DCHECK_GT(request.remaining_requests, 0);
  if (--request.remaining_requests == 0) {
    base::ThreadTaskRunnerHandle::Get()->PostTask(
        FROM_HERE, base::BindOnce(std::move(request.callback), request.result));
    synchronize_requests_.erase(source);
  }
}

}  // namespace web_app
