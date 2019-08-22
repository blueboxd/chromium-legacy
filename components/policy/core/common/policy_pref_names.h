// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_POLICY_CORE_COMMON_POLICY_PREF_NAMES_H_
#define COMPONENTS_POLICY_CORE_COMMON_POLICY_PREF_NAMES_H_

#include "components/policy/policy_export.h"

namespace policy {
namespace policy_prefs {

POLICY_EXPORT extern const char kLastPolicyStatisticsUpdate[];
POLICY_EXPORT extern const char kMachineLevelUserCloudPolicyEnrollmentToken[];
POLICY_EXPORT extern const char kSafeSitesFilterBehavior[];
POLICY_EXPORT extern const char kUrlBlacklist[];
POLICY_EXPORT extern const char kUrlWhitelist[];
POLICY_EXPORT extern const char kUserPolicyRefreshRate[];
POLICY_EXPORT extern const char kCloudManagementEnrollmentMandatory[];
POLICY_EXPORT extern const char kCloudPolicyOverridesPlatformPolicy[];
POLICY_EXPORT extern const char kUnsafeEventsReportingEnabled[];
POLICY_EXPORT extern const char kBlockLargeFileTransfer[];
POLICY_EXPORT extern const char kDelayDeliveryUntilVerdict[];
POLICY_EXPORT extern const char kAllowPasswordProtectedFiles[];
POLICY_EXPORT extern const char kCheckContentCompliance[];

}  // namespace policy_prefs
}  // namespace policy

#endif  // COMPONENTS_POLICY_CORE_COMMON_POLICY_PREF_NAMES_H_
