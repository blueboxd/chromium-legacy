// Copyright (c) 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_SERVICES_APP_SERVICE_PUBLIC_CPP_INTENT_FILTER_UTIL_H_
#define CHROME_SERVICES_APP_SERVICE_PUBLIC_CPP_INTENT_FILTER_UTIL_H_

// Utility functions for providing an App Service intent filter.

#include <string>

#include "base/macros.h"
#include "chrome/services/app_service/public/mojom/types.mojom.h"

namespace apps_util {

apps::mojom::ConditionValuePtr MakeConditionValue(
    const std::string& value,
    apps::mojom::PatternMatchType pattern_match_type);

// Creates condition that makes up App Service intent filter.
apps::mojom::ConditionPtr MakeCondition(
    apps::mojom::ConditionType condition_type,
    std::vector<apps::mojom::ConditionValuePtr> condition_values);

}  // namespace apps_util

#endif  // CHROME_SERVICES_APP_SERVICE_PUBLIC_CPP_INTENT_FILTER_UTIL_H_
