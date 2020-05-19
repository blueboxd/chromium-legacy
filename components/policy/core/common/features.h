// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_POLICY_CORE_COMMON_FEATURES_H_
#define COMPONENTS_POLICY_CORE_COMMON_FEATURES_H_

#include "base/feature_list.h"
#include "build/build_config.h"
#include "components/policy/policy_export.h"

namespace policy {
namespace features {

// TODO(994227) Remove references to this now unused feature.
POLICY_EXPORT extern const base::Feature kPolicyAtomicGroup;

// Feature that controls whether the browser reads the service account
// information from policy data.
POLICY_EXPORT extern const base::Feature kCBCMServiceAccounts;

#if defined(OS_MACOSX)
// Feature that controls whether the browser ignores sensitive policies on an
// unmanaged Mac.
POLICY_EXPORT extern const base::Feature kIgnoreSensitivePoliciesOnUnmanagedMac;
#endif

}  // namespace features
}  // namespace policy

#endif  // COMPONENTS_POLICY_CORE_COMMON_FEATURES_H_
