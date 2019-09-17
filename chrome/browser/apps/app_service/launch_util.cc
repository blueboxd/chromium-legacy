// Copyright (c) 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/apps/app_service/launch_util.h"

#include "ash/public/cpp/shelf_types.h"
#include "chrome/browser/ui/ash/launcher/chrome_launcher_controller.h"

namespace {

ash::ShelfLaunchSource ConvertLaunchSource(
    apps::mojom::LaunchSource launch_source) {
  switch (launch_source) {
    case apps::mojom::LaunchSource::kUnknown:
    case apps::mojom::LaunchSource::kFromParentalControls:
      return ash::LAUNCH_FROM_UNKNOWN;
    case apps::mojom::LaunchSource::kFromAppListGrid:
    case apps::mojom::LaunchSource::kFromAppListGridContextMenu:
      return ash::LAUNCH_FROM_APP_LIST;
    case apps::mojom::LaunchSource::kFromAppListQuery:
    case apps::mojom::LaunchSource::kFromAppListQueryContextMenu:
    case apps::mojom::LaunchSource::kFromAppListRecommendation:
      return ash::LAUNCH_FROM_APP_LIST_SEARCH;
    case apps::mojom::LaunchSource::kFromShelf:
      return ash::LAUNCH_FROM_SHELF;
  }
}

}  // namespace

namespace apps_util {

void Launch(const std::string& app_id,
            int32_t event_flags,
            apps::mojom::LaunchSource launch_source,
            int64_t display_id) {
  ChromeLauncherController::instance()->LaunchApp(
      ash::ShelfID(app_id), ConvertLaunchSource(launch_source), event_flags,
      display_id);
}

}  // namespace apps_util
