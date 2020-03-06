// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/policy/core/common/cloud/enterprise_metrics.h"

namespace policy {

const char kMetricUserPolicyRefresh[] = "Enterprise.PolicyRefresh2";
const char kMetricUserPolicyRefreshFcm[] =
    "Enterprise.FCMInvalidationService.PolicyRefresh2";

const char kMetricUserPolicyInvalidations[] = "Enterprise.PolicyInvalidations";
const char kMetricUserPolicyInvalidationsFcm[] =
    "Enterprise.FCMInvalidationService.PolicyInvalidations";

const char kMetricUserPolicyChromeOSSessionAbort[] =
    "Enterprise.UserPolicyChromeOS.SessionAbort";

const char kMetricDevicePolicyRefresh[] = "Enterprise.DevicePolicyRefresh3";
const char kMetricDevicePolicyRefreshFcm[] =
    "Enterprise.FCMInvalidationService.DevicePolicyRefresh3";

const char kMetricDevicePolicyInvalidations[] =
    "Enterprise.DevicePolicyInvalidations2";
const char kMetricDevicePolicyInvalidationsFcm[] =
    "Enterprise.FCMInvalidationService.DevicePolicyInvalidations2";

const char kMetricDeviceLocalAccountPolicyRefresh[] =
    "Enterprise.DeviceLocalAccountPolicyRefresh3";
const char kMetricDeviceLocalAccountPolicyRefreshFcm[] =
    "Enterprise.FCMInvalidationService.DeviceLocalAccountPolicyRefresh3";

const char kMetricDeviceLocalAccountPolicyInvalidations[] =
    "Enterprise.DeviceLocalAccountPolicyInvalidations2";
const char kMetricDeviceLocalAccountPolicyInvalidationsFcm[] =
    "Enterprise.FCMInvalidationService.DeviceLocalAccountPolicyInvalidations2";

const char kMetricPolicyInvalidationRegistration[] =
    "Enterprise.PolicyInvalidationsRegistrationResult";
const char kMetricPolicyInvalidationRegistrationFcm[] =
    "Enterprise.FCMInvalidationService.PolicyInvalidationsRegistrationResult";

}  // namespace policy
