// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_APPS_APP_SERVICE_APP_SERVICE_METRICS_H_
#define CHROME_BROWSER_APPS_APP_SERVICE_APP_SERVICE_METRICS_H_

#include <map>
#include <string>

#include "chrome/services/app_service/public/mojom/types.mojom.h"

namespace apps {

// The app's histogram name. This is used for logging so do not change the
// order of this enum.
enum class DefaultAppName {
  kCalculator = 0,
  kText = 1,
  kGetHelp = 2,
  kGallery = 3,
  kVideoPlayer = 4,
  kAudioPlayer = 5,
  kChromeCanvas = 6,
  kCamera = 7,
  // Add any new values above this one, and update kMaxValue to the highest
  // enumerator value.
  kMaxValue = kCamera,
};

void RecordDefaultAppLaunch(DefaultAppName default_app_name,
                            apps::mojom::LaunchSource launch_source);

void RecordAppLaunch(const std::string& app_id,
                     apps::mojom::LaunchSource launch_source);

}  // namespace apps

#endif  // CHROME_BROWSER_APPS_APP_SERVICE_APP_SERVICE_METRICS_H_
