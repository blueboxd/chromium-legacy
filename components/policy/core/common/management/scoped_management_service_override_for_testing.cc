// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/policy/core/common/management/scoped_management_service_override_for_testing.h"

namespace policy {

ScopedManagementServiceOverrideForTesting::
    ScopedManagementServiceOverrideForTesting(
        ManagementTarget target,
        base::flat_set<EnterpriseManagementAuthority> authorities)
    : target_(target) {
  ManagementService::SetManagementAuthoritiesForTesting(target,
                                                        std::move(authorities));
}

ScopedManagementServiceOverrideForTesting::
    ~ScopedManagementServiceOverrideForTesting() {
  ManagementService::RemoveManagementAuthoritiesForTesting(target_);
}

}  // namespace policy
