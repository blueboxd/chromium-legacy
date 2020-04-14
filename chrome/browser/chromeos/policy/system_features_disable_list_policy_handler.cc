// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "system_features_disable_list_policy_handler.h"

#include "ash/public/cpp/ash_pref_names.h"
#include "base/values.h"
#include "components/policy/core/common/policy_pref_names.h"
#include "components/policy/policy_constants.h"
#include "components/prefs/pref_registry_simple.h"

namespace policy {

SystemFeaturesDisableListPolicyHandler::SystemFeaturesDisableListPolicyHandler()
    : policy::ListPolicyHandler(key::kSystemFeaturesDisableList,
                                base::Value::Type::STRING) {}

SystemFeaturesDisableListPolicyHandler::
    ~SystemFeaturesDisableListPolicyHandler() = default;

void SystemFeaturesDisableListPolicyHandler::RegisterPrefs(
    PrefRegistrySimple* registry) {
  registry->RegisterListPref(policy_prefs::kSystemFeaturesDisableList);
}

void SystemFeaturesDisableListPolicyHandler::ApplyList(
    base::Value filtered_list,
    PrefValueMap* prefs) {
  DCHECK(filtered_list.is_list());
  base::Value enums_list(base::Value::Type::LIST);
  bool os_settings_enabled = true;
  for (const auto& element : filtered_list.GetList()) {
    SystemFeature feature = ConvertToEnum(element.GetString());
    enums_list.Append(feature);
    if (feature == SystemFeature::OS_SETTINGS)
      os_settings_enabled = false;
  }
  prefs->SetValue(policy_prefs::kSystemFeaturesDisableList,
                  std::move(enums_list));
  prefs->SetBoolean(ash::prefs::kOsSettingsEnabled, os_settings_enabled);
}

SystemFeature SystemFeaturesDisableListPolicyHandler::ConvertToEnum(
    const std::string& system_feature) {
  if (system_feature == "camera")
    return SystemFeature::CAMERA;
  if (system_feature == "os_settings")
    return SystemFeature::OS_SETTINGS;
  if (system_feature == "browser_settings")
    return SystemFeature::BROWSER_SETTINGS;

  NOTREACHED() << "Unsupported system feature: " << system_feature;
  return LAST_SYSTEM_FEATURE;
}

}  // namespace policy
