// Copyright (c) 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/services/app_service/public/cpp/intent_filter_util.h"

namespace apps_util {

apps::mojom::ConditionValuePtr MakeConditionValue(
    const std::string& value,
    apps::mojom::PatternMatchType pattern_match_type) {
  auto condition_value = apps::mojom::ConditionValue::New();
  condition_value->value = value;
  condition_value->match_type = pattern_match_type;

  return condition_value;
}

apps::mojom::ConditionPtr MakeCondition(
    apps::mojom::ConditionType condition_type,
    std::vector<apps::mojom::ConditionValuePtr> condition_values) {
  auto condition = apps::mojom::Condition::New();
  condition->condition_type = condition_type;
  condition->condition_values = std::move(condition_values);

  return condition;
}
}  // namespace apps_util
