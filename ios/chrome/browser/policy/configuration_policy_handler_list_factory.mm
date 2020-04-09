// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/policy/configuration_policy_handler_list_factory.h"

#include "base/bind.h"
#include "base/logging.h"
#include "components/bookmarks/managed/managed_bookmarks_policy_handler.h"
#include "components/password_manager/core/common/password_manager_pref_names.h"
#include "components/policy/core/browser/configuration_policy_handler.h"
#include "components/policy/core/browser/configuration_policy_handler_list.h"
#include "components/policy/core/browser/configuration_policy_handler_parameters.h"
#include "components/policy/policy_constants.h"
#include "ios/chrome/browser/policy/policy_features.h"
#include "ios/chrome/browser/pref_names.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

using policy::PolicyToPreferenceMapEntry;
using policy::SimplePolicyHandler;

namespace {

const PolicyToPreferenceMapEntry kSimplePolicyMap[] = {
    {policy::key::kPasswordManagerEnabled,
     password_manager::prefs::kCredentialsEnableService,
     base::Value::Type::BOOLEAN},
    {policy::key::kSearchSuggestEnabled, prefs::kSearchSuggestEnabled,
     base::Value::Type::BOOLEAN},
};

void PopulatePolicyHandlerParameters(
    policy::PolicyHandlerParameters* parameters) {}

}  // namespace

std::unique_ptr<policy::ConfigurationPolicyHandlerList> BuildPolicyHandlerList(
    const policy::Schema& chrome_schema) {
  DCHECK(IsEnterprisePolicyEnabled());
  std::unique_ptr<policy::ConfigurationPolicyHandlerList> handlers =
      std::make_unique<policy::ConfigurationPolicyHandlerList>(
          base::Bind(&PopulatePolicyHandlerParameters),
          base::Bind(&policy::GetChromePolicyDetails));

  // Check the feature flag before adding handlers to the list.
  if (ShouldInstallEnterprisePolicyHandlers()) {
    for (size_t i = 0; i < base::size(kSimplePolicyMap); ++i) {
      handlers->AddHandler(std::make_unique<SimplePolicyHandler>(
          kSimplePolicyMap[i].policy_name, kSimplePolicyMap[i].preference_path,
          kSimplePolicyMap[i].value_type));
    }

    if (ShouldInstallManagedBookmarksPolicyHandler()) {
      handlers->AddHandler(
          std::make_unique<bookmarks::ManagedBookmarksPolicyHandler>(
              chrome_schema));
    }
  }

  return handlers;
}
