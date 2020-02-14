// Copyright (c) 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/apps/app_service/paused_apps.h"

#include "base/stl_util.h"

namespace apps {

PausedApps::PausedApps() = default;

PausedApps::~PausedApps() = default;

bool PausedApps::MaybeAddApp(const std::string& app_id) {
  auto ret = paused_apps_.insert(app_id);
  return ret.second;
}

bool PausedApps::MaybeRemoveApp(const std::string& app_id) {
  return paused_apps_.erase(app_id) != 0;
}

bool PausedApps::IsPaused(const std::string& app_id) {
  return base::Contains(paused_apps_, app_id);
}

}  // namespace apps
