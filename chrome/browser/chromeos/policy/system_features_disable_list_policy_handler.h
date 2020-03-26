// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_POLICY_SYSTEM_FEATURES_DISABLE_LIST_POLICY_HANDLER_H_
#define CHROME_BROWSER_CHROMEOS_POLICY_SYSTEM_FEATURES_DISABLE_LIST_POLICY_HANDLER_H_

#include <memory>

#include "base/values.h"
#include "components/policy/core/browser/configuration_policy_handler.h"

class PrefValueMap;
class PrefRegistrySimple;

namespace policy {

class SystemFeaturesDisableListPolicyHandler
    : public policy::ListPolicyHandler {
 public:
  SystemFeaturesDisableListPolicyHandler();
  ~SystemFeaturesDisableListPolicyHandler() override;

  static void RegisterPrefs(PrefRegistrySimple* registry);

 protected:
  // ListPolicyHandler:
  void ApplyList(base::Value filtered_list, PrefValueMap* prefs) override;
};

}  // namespace policy

#endif  // CHROME_BROWSER_CHROMEOS_POLICY_SYSTEM_FEATURES_DISABLE_LIST_POLICY_HANDLER_H_
