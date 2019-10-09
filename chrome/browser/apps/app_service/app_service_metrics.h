// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_APPS_APP_SERVICE_APP_SERVICE_METRICS_H_
#define CHROME_BROWSER_APPS_APP_SERVICE_APP_SERVICE_METRICS_H_

#include <map>
#include <string>

#include "chrome/services/app_service/public/mojom/types.mojom.h"

namespace apps {

void RecordAppLaunch(const std::string& app_id,
                     apps::mojom::LaunchSource launch_source);

}  // namespace apps

#endif  // CHROME_BROWSER_APPS_APP_SERVICE_APP_SERVICE_METRICS_H_
