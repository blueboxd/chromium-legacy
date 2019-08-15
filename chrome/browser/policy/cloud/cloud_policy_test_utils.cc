// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/policy/cloud/cloud_policy_test_utils.h"
#include "base/memory/ptr_util.h"
#include "base/values.h"
#include "build/build_config.h"
#include "components/policy/core/common/policy_map.h"
#include "components/policy/core/common/policy_types.h"
#include "components/policy/policy_constants.h"

namespace policy {

void GetExpectedDefaultPolicy(PolicyMap* policy_map) {
  policy_map->ApplyEnterpriseUsersDefaults(GetEnterpriseUsersDefaults());
}


}  // namespace policy
