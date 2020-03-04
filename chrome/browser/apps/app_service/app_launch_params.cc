// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/apps/app_service/app_launch_params.h"

namespace apps {

AppLaunchParams::AppLaunchParams(const std::string& app_id,
                                 apps::mojom::LaunchContainer container,
                                 WindowOpenDisposition disposition,
                                 apps::mojom::AppLaunchSource source,
                                 int64_t display_id)
    : app_id(app_id),
      container(container),
      disposition(disposition),
      command_line(base::CommandLine::NO_PROGRAM),
      source(source),
      display_id(display_id) {}

AppLaunchParams::AppLaunchParams(const AppLaunchParams& other) = default;

AppLaunchParams::~AppLaunchParams() = default;

AppLaunchParams CreateAppIdLaunchParamsWithEventFlags(
    const std::string& app_id,
    int event_flags,
    apps::mojom::AppLaunchSource source,
    int64_t display_id,
    apps::mojom::LaunchContainer fallback_container) {
  WindowOpenDisposition raw_disposition =
      ui::DispositionFromEventFlags(event_flags);

  apps::mojom::LaunchContainer container;
  WindowOpenDisposition disposition;
  if (raw_disposition == WindowOpenDisposition::NEW_FOREGROUND_TAB ||
      raw_disposition == WindowOpenDisposition::NEW_BACKGROUND_TAB) {
    container = apps::mojom::LaunchContainer::kLaunchContainerTab;
    disposition = raw_disposition;
  } else if (raw_disposition == WindowOpenDisposition::NEW_WINDOW) {
    container = apps::mojom::LaunchContainer::kLaunchContainerWindow;
    disposition = raw_disposition;
  } else {
    // Look at preference to find the right launch container.  If no preference
    // is set, launch as a regular tab.
    container = fallback_container;
    disposition = WindowOpenDisposition::NEW_FOREGROUND_TAB;
  }
  return AppLaunchParams(app_id, container, disposition, source, display_id);
}

}  // namespace apps
