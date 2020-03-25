// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "system_features_disable_list_policy_handler.h"

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
    std::unique_ptr<base::ListValue> filtered_list,
    PrefValueMap* prefs) {
  prefs->SetValue(policy_prefs::kSystemFeaturesDisableList,
                  base::Value::FromUniquePtrValue(std::move(filtered_list)));
}

}  // namespace policy
