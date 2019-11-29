// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/sync/driver/sync_policy_handler.h"

#include "base/values.h"
#include "components/policy/core/common/policy_map.h"
#include "components/policy/policy_constants.h"
#include "components/prefs/pref_value_map.h"
#include "components/sync/base/pref_names.h"
#include "components/sync/base/sync_prefs.h"
#include "components/sync/base/user_selectable_type.h"

namespace syncer {

SyncPolicyHandler::SyncPolicyHandler()
    : policy::TypeCheckingPolicyHandler(policy::key::kSyncDisabled,
                                        base::Value::Type::BOOLEAN) {}

SyncPolicyHandler::~SyncPolicyHandler() {}

void SyncPolicyHandler::ApplyPolicySettings(const policy::PolicyMap& policies,
                                            PrefValueMap* prefs) {
  const base::Value* disable_sync_value = policies.GetValue(policy_name());
  bool disable_sync;
  if (disable_sync_value && disable_sync_value->GetAsBoolean(&disable_sync) &&
      disable_sync) {
    prefs->SetValue(prefs::kSyncManaged, disable_sync_value->Clone());
  }

  const base::Value* disabled_sync_types_value =
      policies.GetValue(policy::key::kSyncTypesListDisabled);

  if (disabled_sync_types_value && disabled_sync_types_value->is_list()) {
    auto list = disabled_sync_types_value->GetList();
    bool has_one_valid_entry = false;
    for (auto it = list.begin(); it != list.end(); ++it) {
      if (!it->is_string())
        continue;
      const char* pref = SyncPrefs::GetPrefNameForType(
          GetUserSelectableTypeFromString(it->GetString()));
      if (!pref)
        continue;
      prefs->SetValue(pref, base::Value(false));
      has_one_valid_entry = true;
    }
  }
}

}  // namespace syncer
