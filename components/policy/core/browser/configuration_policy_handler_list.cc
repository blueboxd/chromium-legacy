// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/policy/core/browser/configuration_policy_handler_list.h"

#include "base/bind.h"
#include "components/policy/core/browser/configuration_policy_handler.h"
#include "components/policy/core/browser/configuration_policy_handler_parameters.h"
#include "components/policy/core/browser/policy_error_map.h"
#include "components/policy/core/common/values_util.h"
#include "components/policy/policy_constants.h"
#include "components/prefs/pref_value_map.h"
#include "components/strings/grit/components_strings.h"

namespace policy {

namespace {

// Don't show errors for policies starting with that prefix.
const char kPolicyCommentPrefix[] = "_comment";

}  // namespace

ConfigurationPolicyHandlerList::ConfigurationPolicyHandlerList(
    const PopulatePolicyHandlerParametersCallback& parameters_callback,
    const GetChromePolicyDetailsCallback& details_callback)
    : parameters_callback_(parameters_callback),
      details_callback_(details_callback) {}

ConfigurationPolicyHandlerList::~ConfigurationPolicyHandlerList() {
}

void ConfigurationPolicyHandlerList::AddHandler(
    std::unique_ptr<ConfigurationPolicyHandler> handler) {
  handlers_.push_back(std::move(handler));
}

void ConfigurationPolicyHandlerList::ApplyPolicySettings(
    const PolicyMap& policies,
    PrefValueMap* prefs,
    PolicyErrorMap* errors,
    DeprecatedPoliciesSet* deprecated_policies) const {
  // This function is used both to apply the policy settings, and to check them
  // and list errors. As such it must get all the errors even if it isn't
  // applying the policies.
  // TODO(aberent): split into two functions.
  // TODO(crbug.com/1076560): Returns filtered out future policies for better
  // user interface.
  // TODO(crbug.com/1076560): Provides a way to all whitelist experimental
  // policies in Canary/Dev and test.
  std::unique_ptr<PolicyMap> filtered_policies = policies.DeepCopy();
  base::flat_set<std::string> enabled_future_policies =
      ValueToStringSet(policies.GetValue(key::kEnableExperimentalPolicies));
  filtered_policies->EraseMatching(base::BindRepeating(
      &ConfigurationPolicyHandlerList::FilterOutUnsupportedPolicies,
      base::Unretained(this), enabled_future_policies));

  PolicyErrorMap scoped_errors;
  if (!errors)
    errors = &scoped_errors;

  PolicyHandlerParameters parameters;
  if (parameters_callback_)
    parameters_callback_.Run(&parameters);

  for (const auto& handler : handlers_) {
    if (handler->CheckPolicySettings(*filtered_policies, errors) && prefs) {
      handler->ApplyPolicySettingsWithParameters(
          *filtered_policies, parameters, prefs);
    }
  }

  if (details_callback_ && deprecated_policies) {
    for (const auto& key_value : *filtered_policies) {
      const PolicyDetails* details = details_callback_.Run(key_value.first);
      if (details && details->is_deprecated)
        deprecated_policies->insert(key_value.first);
    }
  }
}

void ConfigurationPolicyHandlerList::PrepareForDisplaying(
    PolicyMap* policies) const {
  for (const auto& handler : handlers_)
    handler->PrepareForDisplaying(policies);
}

bool ConfigurationPolicyHandlerList::FilterOutUnsupportedPolicies(
    const base::flat_set<std::string>& enabled_future_policies,
    const PolicyMap::const_iterator iter) const {
  // Callback might be missing in tests.
  if (!details_callback_) {
    return false;
  }

  const PolicyDetails* policy_details = details_callback_.Run(iter->first);
  if (!policy_details) {
    const std::string prefix(kPolicyCommentPrefix);
    if (iter->first.compare(0, prefix.length(), prefix) != 0) {
      DVLOG(1) << "Unknown policy: " << iter->first;
    }
    return false;
  }

  return IsPlatformDevicePolicy(*policy_details, iter) ||
         IsFuturePolicy(enabled_future_policies, *policy_details, iter);
}

bool ConfigurationPolicyHandlerList::IsPlatformDevicePolicy(
    const PolicyDetails& policy_details,
    const PolicyMap::const_iterator iter) const {
  if (iter->second.source == POLICY_SOURCE_PLATFORM &&
      policy_details.is_device_policy) {
    // Device Policy is only implemented as Cloud Policy (not Platform Policy).
    LOG(WARNING) << "Ignoring device platform policy: " << iter->first;
    return true;
  }
  return false;
}

bool ConfigurationPolicyHandlerList::IsFuturePolicy(
    const base::flat_set<std::string>& enabled_future_policies,
    const PolicyDetails& policy_details,
    const PolicyMap::const_iterator iter) const {
  return policy_details.is_future &&
         !enabled_future_policies.contains(iter->first);
}

}  // namespace policy
