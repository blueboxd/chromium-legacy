// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/apps/app_service/app_service_metrics.h"

#include "base/metrics/histogram_macros.h"
#include "chrome/browser/chromeos/extensions/default_web_app_ids.h"
#include "chrome/browser/chromeos/file_manager/app_id.h"
#include "chrome/common/extensions/extension_constants.h"
#include "chrome/services/app_service/public/mojom/app_service.mojom.h"
#include "extensions/common/constants.h"

namespace apps {

void RecordDefaultAppLaunch(DefaultAppName default_app_name,
                            apps::mojom::LaunchSource launch_source) {
  switch (launch_source) {
    case apps::mojom::LaunchSource::kUnknown:
    case apps::mojom::LaunchSource::kFromParentalControls:
      return;
    case apps::mojom::LaunchSource::kFromAppListGrid:
      UMA_HISTOGRAM_ENUMERATION("Apps.DefaultAppLaunch.FromAppListGrid",
                                default_app_name);
      break;
    case apps::mojom::LaunchSource::kFromAppListGridContextMenu:
      UMA_HISTOGRAM_ENUMERATION(
          "Apps.DefaultAppLaunch.FromAppListGridContextMenu", default_app_name);
      break;
    case apps::mojom::LaunchSource::kFromAppListQuery:
      UMA_HISTOGRAM_ENUMERATION("Apps.DefaultAppLaunch.FromAppListQuery",
                                default_app_name);
      break;
    case apps::mojom::LaunchSource::kFromAppListQueryContextMenu:
      UMA_HISTOGRAM_ENUMERATION(
          "Apps.DefaultAppLaunch.FromAppListQueryContextMenu",
          default_app_name);
      break;
    case apps::mojom::LaunchSource::kFromAppListRecommendation:
      UMA_HISTOGRAM_ENUMERATION(
          "Apps.DefaultAppLaunch.FromAppListRecommendation", default_app_name);
      break;
    case apps::mojom::LaunchSource::kFromShelf:
      UMA_HISTOGRAM_ENUMERATION("Apps.DefaultAppLaunch.FromShelf",
                                default_app_name);
      break;
    case apps::mojom::LaunchSource::kFromFileManager:
      UMA_HISTOGRAM_ENUMERATION("Apps.DefaultAppLaunch.FromFileManager",
                                default_app_name);
      break;
  }
}

void RecordAppLaunch(const std::string& app_id,
                     apps::mojom::LaunchSource launch_source) {
  if (app_id == extension_misc::kCalculatorAppId)
    RecordDefaultAppLaunch(DefaultAppName::kCalculator, launch_source);
  else if (app_id == extension_misc::kTextEditorAppId)
    RecordDefaultAppLaunch(DefaultAppName::kText, launch_source);
  else if (app_id == extension_misc::kGeniusAppId)
    RecordDefaultAppLaunch(DefaultAppName::kGetHelp, launch_source);
  else if (app_id == file_manager::kGalleryAppId)
    RecordDefaultAppLaunch(DefaultAppName::kGallery, launch_source);
  else if (app_id == file_manager::kVideoPlayerAppId)
    RecordDefaultAppLaunch(DefaultAppName::kVideoPlayer, launch_source);
  else if (app_id == file_manager::kAudioPlayerAppId)
    RecordDefaultAppLaunch(DefaultAppName::kAudioPlayer, launch_source);
  else if (app_id == chromeos::default_web_apps::kCanvasAppId)
    RecordDefaultAppLaunch(DefaultAppName::kChromeCanvas, launch_source);
  else if (app_id == extension_misc::kCameraAppId)
    RecordDefaultAppLaunch(DefaultAppName::kCamera, launch_source);
}

}  // namespace apps
