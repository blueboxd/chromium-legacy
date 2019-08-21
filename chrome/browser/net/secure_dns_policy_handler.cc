// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/net/secure_dns_policy_handler.h"

#include "base/strings/string_number_conversions.h"
#include "base/values.h"
#include "chrome/browser/net/dns_util.h"
#include "chrome/browser/prefs/session_startup_pref.h"
#include "chrome/common/pref_names.h"
#include "components/policy/core/browser/policy_error_map.h"
#include "components/policy/core/common/policy_map.h"
#include "components/policy/policy_constants.h"
#include "components/prefs/pref_value_map.h"
#include "components/strings/grit/components_strings.h"

namespace policy {

SecureDnsPolicyHandler::SecureDnsPolicyHandler() {}

SecureDnsPolicyHandler::~SecureDnsPolicyHandler() {}

bool SecureDnsPolicyHandler::CheckPolicySettings(const PolicyMap& policies,
                                                 PolicyErrorMap* errors) {
  const base::Value* mode = policies.GetValue(key::kDnsOverHttpsMode);
  if (!mode)
    return false;

  std::string mode_str;
  if (!mode->GetAsString(&mode_str)) {
    errors->AddError(key::kDnsOverHttpsMode, IDS_POLICY_TYPE_ERROR,
                     base::Value::GetTypeName(base::Value::Type::STRING));
    return false;
  } else if (mode_str.size() == 0) {
    errors->AddError(key::kDnsOverHttpsMode, IDS_POLICY_NOT_SPECIFIED_ERROR);
    return false;
  } else if (mode_str == kDnsOverHttpsModeSecure) {
    errors->AddError(key::kDnsOverHttpsMode,
                     IDS_POLICY_SECURE_DNS_MODE_NOT_SUPPORTED_ERROR);
  } else if (mode_str != kDnsOverHttpsModeOff &&
             mode_str != kDnsOverHttpsModeAutomatic) {
    errors->AddError(key::kDnsOverHttpsMode,
                     IDS_POLICY_INVALID_SECURE_DNS_MODE_ERROR);
    return false;
  }

  // Try to apply any setting that is valid
  return true;
}

void SecureDnsPolicyHandler::ApplyPolicySettings(const PolicyMap& policies,
                                                 PrefValueMap* prefs) {
  const base::Value* mode = policies.GetValue(key::kDnsOverHttpsMode);

  std::string mode_str = mode->GetString();
  // TODO(http://crbug.com/955454): Include secure in conditional when
  // support is implemented.
  if (mode_str == kDnsOverHttpsModeAutomatic) {
    prefs->SetString(prefs::kDnsOverHttpsMode, mode_str);
  } else {
    // Captures "off" and "secure".
    prefs->SetString(prefs::kDnsOverHttpsMode, kDnsOverHttpsModeOff);
  }
}

}  // namespace policy
