// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/child_accounts/time_limits/app_time_limit_utils.h"

#include "chrome/browser/chromeos/child_accounts/time_limits/app_types.h"
#include "chrome/common/extensions/extension_constants.h"
#include "chrome/services/app_service/public/mojom/types.mojom.h"
#include "extensions/common/constants.h"

namespace chromeos {
namespace app_time {

AppId GetChromeAppId() {
  return AppId(apps::mojom::AppType::kExtension, extension_misc::kChromeAppId);
}

bool IsWebAppOrExtension(const AppId& app_id) {
  return app_id.app_type() == apps::mojom::AppType::kWeb ||
         app_id.app_type() == apps::mojom::AppType::kExtension;
}

// Returns true if the application shares chrome's time limit.
bool ContributesToWebTimeLimit(const AppId& app_id, AppState state) {
  if (state == AppState::kAlwaysAvailable)
    return false;

  return IsWebAppOrExtension(app_id);
}

}  // namespace app_time
}  // namespace chromeos
