// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/web_applications/components/web_app_constants.h"

#include "base/compiler_specific.h"
#include "components/services/app_service/public/mojom/types.mojom.h"

namespace web_app {

static_assert(Source::kMinValue == 0, "Source enum should be zero based");

bool IsSuccess(InstallResultCode code) {
  return code == InstallResultCode::kSuccessNewInstall ||
         code == InstallResultCode::kSuccessAlreadyInstalled;
}

DisplayMode ResolveEffectiveDisplayMode(DisplayMode app_display_mode,
                                        DisplayMode user_display_mode) {
  switch (user_display_mode) {
    case DisplayMode::kBrowser:
      return user_display_mode;
    case DisplayMode::kUndefined:
    case DisplayMode::kMinimalUi:
    case DisplayMode::kFullscreen:
      NOTREACHED();
      FALLTHROUGH;
    case DisplayMode::kStandalone:
      break;
  }

  switch (app_display_mode) {
    case DisplayMode::kBrowser:
    case DisplayMode::kMinimalUi:
      return DisplayMode::kMinimalUi;
    case DisplayMode::kUndefined:
      NOTREACHED();
      FALLTHROUGH;
    case DisplayMode::kStandalone:
    case DisplayMode::kFullscreen:
      return DisplayMode::kStandalone;
  }
}

apps::mojom::LaunchContainer ConvertDisplayModeToAppLaunchContainer(
    DisplayMode display_mode) {
  switch (display_mode) {
    case DisplayMode::kBrowser:
      return apps::mojom::LaunchContainer::kLaunchContainerTab;
    case DisplayMode::kMinimalUi:
      return apps::mojom::LaunchContainer::kLaunchContainerWindow;
    case DisplayMode::kStandalone:
      return apps::mojom::LaunchContainer::kLaunchContainerWindow;
    case DisplayMode::kFullscreen:
      return apps::mojom::LaunchContainer::kLaunchContainerWindow;
    case DisplayMode::kUndefined:
      return apps::mojom::LaunchContainer::kLaunchContainerNone;
  }
}

}  // namespace web_app
