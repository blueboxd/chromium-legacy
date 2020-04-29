// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>
#include <utility>

#include "chrome/services/app_service/public/cpp/preferred_apps_converter.h"
#include "chrome/services/app_service/public/mojom/types.mojom.h"

namespace {

base::Value ConvertConditionValueToValue(
    const apps::mojom::ConditionValuePtr& condition_value) {
  base::Value condition_value_dict(base::Value::Type::DICTIONARY);
  condition_value_dict.SetStringKey(apps::kValueKey, condition_value->value);
  condition_value_dict.SetIntKey(apps::kMatchTypeKey,
                                 static_cast<int>(condition_value->match_type));
  return condition_value_dict;
}

base::Value ConvertConditionToValue(
    const apps::mojom::ConditionPtr& condition) {
  base::Value condition_dict(base::Value::Type::DICTIONARY);
  condition_dict.SetIntKey(apps::kConditionTypeKey,
                           static_cast<int>(condition->condition_type));
  base::Value condition_values_list(base::Value::Type::LIST);
  for (auto& condition_value : condition->condition_values) {
    condition_values_list.Append(ConvertConditionValueToValue(condition_value));
  }
  condition_dict.SetKey(apps::kConditionValuesKey,
                        std::move(condition_values_list));
  return condition_dict;
}

base::Value ConvertIntentFilterToValue(
    const apps::mojom::IntentFilterPtr& intent_filter) {
  base::Value intent_filter_value(base::Value::Type::LIST);
  for (auto& condition : intent_filter->conditions) {
    intent_filter_value.Append(ConvertConditionToValue(condition));
  }
  return intent_filter_value;
}

apps::mojom::ConditionValuePtr ParseValueToConditionValue(
    const base::Value& value) {
  auto* value_string = value.FindStringKey(apps::kValueKey);
  if (!value_string) {
    DVLOG(0) << "Fail to parse condition value. Cannot find \""
             << apps::kValueKey << "\" key with string value.";
    return nullptr;
  }
  auto condition_value = apps::mojom::ConditionValue::New();
  condition_value->value = *value_string;
  auto match_type = value.FindIntKey(apps::kMatchTypeKey);
  if (!match_type.has_value()) {
    DVLOG(0) << "Fail to parse condition value. Cannot find \""
             << apps::kMatchTypeKey << "\" key with int value.";
    return nullptr;
  }
  condition_value->match_type =
      static_cast<apps::mojom::PatternMatchType>(match_type.value());
  return condition_value;
}

apps::mojom::ConditionPtr ParseValueToCondition(const base::Value& value) {
  auto condition_type = value.FindIntKey(apps::kConditionTypeKey);
  if (!condition_type.has_value()) {
    DVLOG(0) << "Fail to parse condition. Cannot find \""
             << apps::kConditionTypeKey << "\" key with int value.";
    return nullptr;
  }
  auto condition = apps::mojom::Condition::New();
  condition->condition_type =
      static_cast<apps::mojom::ConditionType>(condition_type.value());

  auto* condition_values = value.FindKey(apps::kConditionValuesKey);
  if (!condition_values || !condition_values->is_list()) {
    DVLOG(0) << "Fail to parse condition. Cannot find \""
             << apps::kConditionValuesKey << "\" key with list value.";
    return nullptr;
  }
  for (auto& condition_value : condition_values->GetList()) {
    auto parsed_condition_value = ParseValueToConditionValue(condition_value);
    if (!parsed_condition_value) {
      DVLOG(0) << "Fail to parse condition. Cannot parse condition values";
      return nullptr;
    }
    condition->condition_values.push_back(std::move(parsed_condition_value));
  }
  return condition;
}

apps::mojom::IntentFilterPtr ParseValueToIntentFilter(
    const base::Value* value) {
  if (!value || !value->is_list()) {
    DVLOG(0) << "Fail to parse intent filter. Cannot find the conditions list.";
    return nullptr;
  }
  auto intent_filter = apps::mojom::IntentFilter::New();
  for (auto& condition : value->GetList()) {
    auto parsed_condition = ParseValueToCondition(condition);
    if (!parsed_condition) {
      DVLOG(0) << "Fail to parse intent filter. Cannot parse conditions.";
      return nullptr;
    }
    intent_filter->conditions.push_back(std::move(parsed_condition));
  }
  return intent_filter;
}

}  // namespace

namespace apps {

const char kConditionTypeKey[] = "condition_type";
const char kConditionValuesKey[] = "condition_values";
const char kValueKey[] = "value";
const char kMatchTypeKey[] = "match_type";
const char kAppIdKey[] = "app_id";
const char kIntentFilterKey[] = "intent_filter";

base::Value ConvertPreferredAppsToValue(
    const PreferredAppsList::PreferredApps& preferred_apps) {
  base::Value preferred_apps_value(base::Value::Type::LIST);
  for (auto& preferred_app : preferred_apps) {
    base::Value preferred_app_dict(base::Value::Type::DICTIONARY);
    preferred_app_dict.SetKey(
        kIntentFilterKey,
        ConvertIntentFilterToValue(preferred_app->intent_filter));
    preferred_app_dict.SetStringKey(kAppIdKey, preferred_app->app_id);
    preferred_apps_value.Append(std::move(preferred_app_dict));
  }
  return preferred_apps_value;
}

PreferredAppsList::PreferredApps ParseValueToPreferredApps(
    const base::Value& preferred_apps_value) {
  if (!preferred_apps_value.is_list()) {
    DVLOG(0) << "Fail to parse preferred apps. Cannot the preferred app list.";
    return PreferredAppsList::PreferredApps();
  }
  PreferredAppsList::PreferredApps preferred_apps;
  for (auto& entry : preferred_apps_value.GetList()) {
    auto* app_id = entry.FindStringKey(kAppIdKey);
    if (!app_id) {
      DVLOG(0) << "Fail to parse condition value. Cannot find \""
               << apps::kAppIdKey << "\" key with string value.";
      return PreferredAppsList::PreferredApps();
    }
    auto parsed_intent_filter =
        ParseValueToIntentFilter(entry.FindKey(kIntentFilterKey));
    if (!parsed_intent_filter) {
      DVLOG(0) << "Fail to parse condition value. Cannot parse intent filter.";
      return PreferredAppsList::PreferredApps();
    }
    auto new_preferred_app = apps::mojom::PreferredApp::New(
        std::move(parsed_intent_filter), *app_id);
    preferred_apps.push_back(std::move(new_preferred_app));
  }
  return preferred_apps;
}

}  // namespace apps
