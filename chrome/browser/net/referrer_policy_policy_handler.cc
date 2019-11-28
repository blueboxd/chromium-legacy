// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/net/referrer_policy_policy_handler.h"

#include "base/values.h"
#include "components/policy/core/common/policy_map.h"
#include "components/policy/policy_constants.h"
#include "components/prefs/pref_value_map.h"
#include "content/public/common/referrer.h"

namespace policy {

ReferrerPolicyPolicyHandler::ReferrerPolicyPolicyHandler()
    : TypeCheckingPolicyHandler(policy::key::kForceLegacyDefaultReferrerPolicy,
                                base::Value::Type::BOOLEAN) {}

ReferrerPolicyPolicyHandler::~ReferrerPolicyPolicyHandler() = default;

void ReferrerPolicyPolicyHandler::ApplyPolicySettings(const PolicyMap& policies,
                                                      PrefValueMap* prefs) {
  const base::Value* value =
      policies.GetValue(key::kForceLegacyDefaultReferrerPolicy);
  if (value) {
    DCHECK(value->is_bool());
    content::Referrer::SetForceLegacyDefaultReferrerPolicy(value->GetBool());
  }
}

}  // namespace policy
