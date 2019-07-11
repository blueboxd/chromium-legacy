// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/policy/core/common/cloud/enterprise_metrics.h"

namespace policy {

const char kMetricUserPolicyRefresh[] = "Enterprise.PolicyRefresh";
const char kMetricUserPolicyRefreshFcm[] =
    "Enterprise.FCMInvalidationService.PolicyRefresh";
const char kMetricUserPolicyRefreshTicl[] =
    "Enterprise.TiclInvalidationService.PolicyRefresh";

const char kMetricUserPolicyInvalidations[] = "Enterprise.PolicyInvalidations";
const char kMetricUserPolicyInvalidationsFcm[] =
    "Enterprise.FCMInvalidationService.PolicyInvalidations";
const char kMetricUserPolicyInvalidationsTicl[] =
    "Enterprise.TiclInvalidationService.PolicyInvalidations";

const char kMetricUserPolicyChromeOSSessionAbort[] =
    "Enterprise.UserPolicyChromeOS.SessionAbort";

const char kMetricDevicePolicyRefresh[] = "Enterprise.DevicePolicyRefresh";
const char kMetricDevicePolicyRefreshFcm[] =
    "Enterprise.FCMInvalidationService.DevicePolicyRefresh";
const char kMetricDevicePolicyRefreshTicl[] =
    "Enterprise.TiclInvalidationService.DevicePolicyRefresh";

const char kMetricDevicePolicyInvalidations[] =
    "Enterprise.DevicePolicyInvalidations";
const char kMetricDevicePolicyInvalidationsFcm[] =
    "Enterprise.FCMInvalidationService.DevicePolicyInvalidations";
const char kMetricDevicePolicyInvalidationsTicl[] =
    "Enterprise.TiclInvalidationService.DevicePolicyInvalidations";

const char kMetricPolicyInvalidationRegistration[] =
    "Enterprise.PolicyInvalidationsRegistrationResult";
const char kMetricPolicyInvalidationRegistrationFcm[] =
    "Enterprise.FCMInvalidationService.PolicyInvalidationsRegistrationResult";
const char kMetricPolicyInvalidationRegistrationTicl[] =
    "Enterprise.TiclInvalidationService.PolicyInvalidationsRegistrationResult";

}  // namespace policy
