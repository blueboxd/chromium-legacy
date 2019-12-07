// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/child_accounts/time_limits/app_activity_registry.h"

#include "base/stl_util.h"

namespace chromeos {
namespace app_time {

AppActivityRegistry::AppDetails::AppDetails() = default;

AppActivityRegistry::AppDetails::AppDetails(const AppActivity& activity)
    : activity(activity) {}

AppActivityRegistry::AppDetails::AppDetails(const AppDetails&) = default;

AppActivityRegistry::AppDetails& AppActivityRegistry::AppDetails::operator=(
    const AppDetails&) = default;

AppActivityRegistry::AppDetails::~AppDetails() = default;

AppActivityRegistry::AppActivityRegistry(AppServiceWrapper* app_service_wrapper)
    : app_service_wrapper_(app_service_wrapper) {
  DCHECK(app_service_wrapper_);

  app_service_wrapper_->AddObserver(this);
}

AppActivityRegistry::~AppActivityRegistry() {
  app_service_wrapper_->RemoveObserver(this);
}

void AppActivityRegistry::OnAppInstalled(const AppId& app_id) {
  // App might be already present in registry, because we preserve info between
  // sessions and app service does not. Make sure not to override cached state.
  if (base::Contains(activity_registry_, app_id))
    Add(app_id);
}

void AppActivityRegistry::OnAppUninstalled(const AppId& app_id) {
  // TODO(agawronska): Consider DCHECK instead of it. Not sure if there are
  // legit cases when we might go out of sync with AppService.
  if (base::Contains(activity_registry_, app_id))
    SetAppState(app_id, AppState::kUninstalled);
}

void AppActivityRegistry::OnAppAvailable(const AppId& app_id) {
  if (base::Contains(activity_registry_, app_id))
    SetAppState(app_id, AppState::kAvailable);
}

void AppActivityRegistry::OnAppBlocked(const AppId& app_id) {
  if (base::Contains(activity_registry_, app_id))
    SetAppState(app_id, AppState::kBlocked);
}

void AppActivityRegistry::Add(const AppId& app_id) {
  auto result = activity_registry_.emplace(
      app_id, AppDetails(AppActivity(AppState::kAvailable)));
  DCHECK(result.second);
}

AppState AppActivityRegistry::GetAppState(const AppId& app_id) const {
  DCHECK(base::Contains(activity_registry_, app_id));
  return activity_registry_.at(app_id).activity.app_state();
}

void AppActivityRegistry::SetAppState(const AppId& app_id, AppState app_state) {
  DCHECK(base::Contains(activity_registry_, app_id));
  activity_registry_.at(app_id).activity.SetAppState(app_state);
}

void AppActivityRegistry::CleanRegistry() {
  for (auto it = activity_registry_.cbegin();
       it != activity_registry_.cend();) {
    if (GetAppState(it->first) == AppState::kUninstalled) {
      it = activity_registry_.erase(it);
    } else {
      ++it;
    }
  }
}

}  // namespace app_time
}  // namespace chromeos
