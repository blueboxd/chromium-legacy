// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_NET_REFERRER_POLICY_POLICY_HANDLER_H_
#define CHROME_BROWSER_NET_REFERRER_POLICY_POLICY_HANDLER_H_

#include "components/policy/core/browser/configuration_policy_handler.h"

class PrefValueMap;

namespace policy {

// Handles the ForceLegacyDefaultReferrerPolicy policy
// by updating the pertinent //content global when the policy's set.
class ReferrerPolicyPolicyHandler : public TypeCheckingPolicyHandler {
 public:
  ReferrerPolicyPolicyHandler();
  ~ReferrerPolicyPolicyHandler() override;

  ReferrerPolicyPolicyHandler(const ReferrerPolicyPolicyHandler&) = delete;
  ReferrerPolicyPolicyHandler& operator=(const ReferrerPolicyPolicyHandler&) =
      delete;

  // ConfigurationPolicyHandler methods:
  void ApplyPolicySettings(const PolicyMap& policies,
                           PrefValueMap* prefs) override;
};

}  // namespace policy

#endif  // CHROME_BROWSER_NET_REFERRER_POLICY_POLICY_HANDLER_H_
