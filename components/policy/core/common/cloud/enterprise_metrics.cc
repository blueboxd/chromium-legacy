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

const char kMetricUserRemoteCommandInvalidations[] =
    "Enterprise.UserRemoteCommandInvalidations";
const char kMetricDeviceRemoteCommandInvalidations[] =
    "Enterprise.DeviceRemoteCommandInvalidations";

const char kMetricRemoteCommandInvalidationsRegistrationResult[] =
    "Enterprise.RemoteCommandInvalidationsRegistrationResult";

const char kMetricUserRemoteCommandReceived[] =
    "Enterprise.UserRemoteCommand.Received";

const char kMetricUserUnsignedRemoteCommandReceived[] =
    "Enterprise.UserRemoteCommand.Received.Unsigned";

// Expands to:
// Enterprise.UserRemoteCommand.Executed.CommandEchoTest
// Enterprise.UserRemoteCommand.Executed.DeviceReboot
// Enterprise.UserRemoteCommand.Executed.DeviceScreenshot
// Enterprise.UserRemoteCommand.Executed.DeviceSetVolume
// Enterprise.UserRemoteCommand.Executed.DeviceStartCrdSession
// Enterprise.UserRemoteCommand.Executed.DeviceFetchStatus
// Enterprise.UserRemoteCommand.Executed.UserArcCommand
// Enterprise.UserRemoteCommand.Executed.DeviceWipeUsers
// Enterprise.UserRemoteCommand.Executed.DeviceRefreshEnterpriseMachineCertificate
// Enterprise.UserRemoteCommand.Executed.DeviceRemotePowerwash
// Enterprise.UserRemoteCommand.Executed.DeviceGetAvailableDiagnosticRoutines
// Enterprise.UserRemoteCommand.Executed.DeviceRunDiagnosticRoutine
// Enterprise.UserRemoteCommand.Executed.DeviceGetDiagnosticRoutineUpdate
const char kMetricUserRemoteCommandExecutedTemplate[] =
    "Enterprise.UserRemoteCommand.Executed.%s";

// Expands to:
// Enterprise.UserRemoteCommand.Executed.Unsigned.CommandEchoTest
// Enterprise.UserRemoteCommand.Executed.Unsigned.DeviceReboot
// Enterprise.UserRemoteCommand.Executed.Unsigned.DeviceScreenshot
// Enterprise.UserRemoteCommand.Executed.Unsigned.DeviceSetVolume
// Enterprise.UserRemoteCommand.Executed.Unsigned.DeviceStartCrdSession
// Enterprise.UserRemoteCommand.Executed.Unsigned.DeviceFetchStatus
// Enterprise.UserRemoteCommand.Executed.Unsigned.UserArcCommand
// Enterprise.UserRemoteCommand.Executed.Unsigned.DeviceWipeUsers
// Enterprise.UserRemoteCommand.Executed.Unsigned.DeviceRefreshEnterpriseMachineCertificate
// Enterprise.UserRemoteCommand.Executed.Unsigned.DeviceRemotePowerwash
// Enterprise.UserRemoteCommand.Executed.Unsigned.DeviceGetAvailableDiagnosticRoutines
// Enterprise.UserRemoteCommand.Executed.Unsigned.DeviceRunDiagnosticRoutine
// Enterprise.UserRemoteCommand.Executed.Unsigned.DeviceGetDiagnosticRoutineUpdate
const char kMetricUserUnsignedRemoteCommandExecutedTemplate[] =
    "Enterprise.UserRemoteCommand.Executed.Unsigned.%s";

const char kMetricDeviceRemoteCommandReceived[] =
    "Enterprise.DeviceRemoteCommand.Received";

const char kMetricDeviceUnsignedRemoteCommandReceived[] =
    "Enterprise.DeviceRemoteCommand.Received.Unsigned";

// Expands to:
// Enterprise.DeviceRemoteCommand.Executed.CommandEchoTest
// Enterprise.DeviceRemoteCommand.Executed.DeviceReboot
// Enterprise.DeviceRemoteCommand.Executed.DeviceScreenshot
// Enterprise.DeviceRemoteCommand.Executed.DeviceSetVolume
// Enterprise.DeviceRemoteCommand.Executed.DeviceStartCrdSession
// Enterprise.DeviceRemoteCommand.Executed.DeviceFetchStatus
// Enterprise.DeviceRemoteCommand.Executed.UserArcCommand
// Enterprise.DeviceRemoteCommand.Executed.DeviceWipeUsers
// Enterprise.DeviceRemoteCommand.Executed.DeviceRefreshEnterpriseMachineCertificate
// Enterprise.DeviceRemoteCommand.Executed.DeviceRemotePowerwash
// Enterprise.DeviceRemoteCommand.Executed.DeviceGetAvailableDiagnosticRoutines
// Enterprise.DeviceRemoteCommand.Executed.DeviceRunDiagnosticRoutine
// Enterprise.DeviceRemoteCommand.Executed.DeviceGetDiagnosticRoutineUpdate
const char kMetricDeviceRemoteCommandExecutedTemplate[] =
    "Enterprise.DeviceRemoteCommand.Executed.%s";

// Expands to:
// Enterprise.DeviceRemoteCommand.Executed.Unsigned.CommandEchoTest
// Enterprise.DeviceRemoteCommand.Executed.Unsigned.DeviceReboot
// Enterprise.DeviceRemoteCommand.Executed.Unsigned.DeviceScreenshot
// Enterprise.DeviceRemoteCommand.Executed.Unsigned.DeviceSetVolume
// Enterprise.DeviceRemoteCommand.Executed.Unsigned.DeviceStartCrdSession
// Enterprise.DeviceRemoteCommand.Executed.Unsigned.DeviceFetchStatus
// Enterprise.DeviceRemoteCommand.Executed.Unsigned.UserArcCommand
// Enterprise.DeviceRemoteCommand.Executed.Unsigned.DeviceWipeUsers
// Enterprise.DeviceRemoteCommand.Executed.Unsigned.DeviceRefreshEnterpriseMachineCertificate
// Enterprise.DeviceRemoteCommand.Executed.Unsigned.DeviceRemotePowerwash
// Enterprise.DeviceRemoteCommand.Executed.Unsigned.DeviceGetAvailableDiagnosticRoutines
// Enterprise.DeviceRemoteCommand.Executed.Unsigned.DeviceRunDiagnosticRoutine
// Enterprise.DeviceRemoteCommand.Executed.Unsigned.DeviceGetDiagnosticRoutineUpdate
const char kMetricDeviceUnsignedRemoteCommandExecutedTemplate[] =
    "Enterprise.DeviceRemoteCommand.Executed.Unsigned.%s";

}  // namespace policy
