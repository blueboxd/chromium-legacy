// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/supervised_user/supervised_user_extensions_metrics_recorder.h"

#include "base/check.h"
#include "base/logging.h"
#include "base/metrics/histogram_functions.h"
#include "base/metrics/user_metrics.h"
#include "base/time/default_tick_clock.h"

// static
const char SupervisedUserExtensionsMetricsRecorder::kExtensionsHistogramName[] =
    "SupervisedUsers.Extensions2";
const char
    SupervisedUserExtensionsMetricsRecorder::kApprovalGrantedActionName[] =
        "SupervisedUsers_Extensions_ApprovalGranted";
const char SupervisedUserExtensionsMetricsRecorder::
    kPermissionsIncreaseGrantedActionName[] =
        "SupervisedUsers_Extensions_PermissionsIncreaseGranted";
const char
    SupervisedUserExtensionsMetricsRecorder::kApprovalRemovedActionName[] =
        "SupervisedUsers_Extensions_ApprovalRemoved";
// Extension Install Dialog.
const char SupervisedUserExtensionsMetricsRecorder::
    kExtensionInstallDialogHistogramName[] =
        "SupervisedUsers.ExtensionInstallDialog";
const char SupervisedUserExtensionsMetricsRecorder::
    kExtensionInstallDialogAskedParentTimeHistogramName[] =
        "SupervisedUsers.ExtensionInstallDialog.AskedParentUserTime";
const char SupervisedUserExtensionsMetricsRecorder::
    kExtensionInstallDialogChildCanceledTimeHistogramName[] =
        "SupervisedUsers.ExtensionInstallDialog.ChildCanceledUserTime";
const char SupervisedUserExtensionsMetricsRecorder::
    kExtensionInstallDialogOpenedActionName[] =
        "SupervisedUsers_Extensions_ExtensionInstallDialog_Opened";
const char SupervisedUserExtensionsMetricsRecorder::
    kExtensionInstallDialogAskedParentActionName[] =
        "SupervisedUsers_Extensions_ExtensionInstallDialog_AskedParent";
const char SupervisedUserExtensionsMetricsRecorder::
    kExtensionInstallDialogChildCanceledActionName[] =
        "SupervisedUsers_Extensions_ExtensionInstallDialog_ChildCanceled";
// Parent Permission Dialog.
const char SupervisedUserExtensionsMetricsRecorder::
    kParentPermissionDialogHistogramName[] =
        "SupervisedUsers.ParentPermissionDialog";
const char SupervisedUserExtensionsMetricsRecorder::
    kParentPermissionDialogParentApprovedTimeHistogramName[] =
        "SupervisedUsers.ParentPermissionDialog.ParentApprovedUserTime";
const char SupervisedUserExtensionsMetricsRecorder::
    kParentPermissionDialogParentCanceledTimeHistogramName[] =
        "SupervisedUsers.ParentPermissionDialog.ParentCanceledUserTime";
const char SupervisedUserExtensionsMetricsRecorder::
    kParentPermissionDialogFailedTimeHistogramName[] =
        "SupervisedUsers.ParentPermissionDialog.FailedUserTime";
const char SupervisedUserExtensionsMetricsRecorder::
    kParentPermissionDialogOpenedActionName[] =
        "SupervisedUsers_Extensions_ParentPermissionDialog_Opened";
const char SupervisedUserExtensionsMetricsRecorder::
    kParentPermissionDialogParentApprovedActionName[] =
        "SupervisedUsers_Extensions_ParentPermissionDialog_ParentApproved";
const char SupervisedUserExtensionsMetricsRecorder::
    kParentPermissionDialogParentCanceledActionName[] =
        "SupervisedUsers_Extensions_ParentPermissionDialog_ParentCanceled";
// Enabling and disabling extensions.
const char SupervisedUserExtensionsMetricsRecorder::kEnablementHistogramName[] =
    "SupervisedUsers.ExtensionEnablement";
const char SupervisedUserExtensionsMetricsRecorder::kEnabledActionName[] =
    "SupervisedUsers_Extensions_Enabled";
const char SupervisedUserExtensionsMetricsRecorder::kDisabledActionName[] =
    "SupervisedUsers_Extensions_Disabled";

SupervisedUserExtensionsMetricsRecorder::
    SupervisedUserExtensionsMetricsRecorder()
    : clock_(base::DefaultTickClock::GetInstance()) {}

void SupervisedUserExtensionsMetricsRecorder::OnDialogOpened() {
  RecordExtensionInstallDialogUmaMetrics(ExtensionInstallDialogState::kOpened);
}

void SupervisedUserExtensionsMetricsRecorder::OnDialogAccepted() {
  RecordExtensionInstallDialogUmaMetrics(
      ExtensionInstallDialogState::kAskedParent);
}

void SupervisedUserExtensionsMetricsRecorder::OnDialogCanceled() {
  RecordExtensionInstallDialogUmaMetrics(
      ExtensionInstallDialogState::kChildCanceled);
}

// static
void SupervisedUserExtensionsMetricsRecorder::RecordExtensionsUmaMetrics(
    UmaExtensionState state) {
  base::UmaHistogramEnumeration(kExtensionsHistogramName, state);
  switch (state) {
    case UmaExtensionState::kApprovalGranted:
      // Record UMA metrics for custodian approval for a new extension.
      base::RecordAction(base::UserMetricsAction(kApprovalGrantedActionName));
      break;
    case UmaExtensionState::kPermissionsIncreaseGranted:
      // Record UMA metrics for child approval for a newer version of an
      // existing extension with increased permissions.
      base::RecordAction(
          base::UserMetricsAction(kPermissionsIncreaseGrantedActionName));
      break;
    case UmaExtensionState::kApprovalRemoved:
      // Record UMA metrics for removing an extension.
      base::RecordAction(base::UserMetricsAction(kApprovalRemovedActionName));
      break;
  }
}

void SupervisedUserExtensionsMetricsRecorder::
    RecordExtensionInstallDialogUmaMetrics(ExtensionInstallDialogState state) {
  base::UmaHistogramEnumeration(kExtensionInstallDialogHistogramName, state);
  switch (state) {
    case ExtensionInstallDialogState::kOpened:
      base::RecordAction(
          base::UserMetricsAction(kExtensionInstallDialogOpenedActionName));
      start_time_ = clock_->NowTicks();
      break;
    case ExtensionInstallDialogState::kAskedParent:
      base::RecordAction(base::UserMetricsAction(
          kExtensionInstallDialogAskedParentActionName));
      RecordUserTime(kExtensionInstallDialogAskedParentTimeHistogramName);
      break;
    case ExtensionInstallDialogState::kChildCanceled:
      base::RecordAction(base::UserMetricsAction(
          kExtensionInstallDialogChildCanceledActionName));
      RecordUserTime(kExtensionInstallDialogChildCanceledTimeHistogramName);
      break;
  }
}

void SupervisedUserExtensionsMetricsRecorder::
    RecordParentPermissionDialogUmaMetrics(ParentPermissionDialogState state) {
  base::UmaHistogramEnumeration(kParentPermissionDialogHistogramName, state);
  switch (state) {
    case ParentPermissionDialogState::kOpened:
      base::RecordAction(
          base::UserMetricsAction(kParentPermissionDialogOpenedActionName));
      start_time_ = clock_->NowTicks();
      break;
    case ParentPermissionDialogState::kParentApproved:
      base::RecordAction(base::UserMetricsAction(
          kParentPermissionDialogParentApprovedActionName));
      RecordUserTime(kParentPermissionDialogParentApprovedTimeHistogramName);
      break;
    case ParentPermissionDialogState::kParentCanceled:
      base::RecordAction(base::UserMetricsAction(
          kParentPermissionDialogParentCanceledActionName));
      RecordUserTime(kParentPermissionDialogParentCanceledTimeHistogramName);
      break;
    case ParentPermissionDialogState::kFailed:
      RecordUserTime(kParentPermissionDialogFailedTimeHistogramName);
      break;
    case ParentPermissionDialogState::kNoParentError:
      // Nothing to do here.
      break;
  }
}

// static
void SupervisedUserExtensionsMetricsRecorder::RecordEnablementUmaMetrics(
    EnablementState state) {
  base::UmaHistogramEnumeration(kEnablementHistogramName, state);
  switch (state) {
    case EnablementState::kEnabled:
      base::RecordAction(base::UserMetricsAction(kEnabledActionName));
      break;
    case EnablementState::kDisabled:
      base::RecordAction(base::UserMetricsAction(kDisabledActionName));
      break;
  }
}

void SupervisedUserExtensionsMetricsRecorder::SetClockForTesting(
    const base::TickClock* tick_clock) {
  clock_ = tick_clock;
}

void SupervisedUserExtensionsMetricsRecorder::RecordUserTime(
    const std::string& metric_name) const {
  DCHECK(!start_time_.is_null()) << "start_time_ has not been initialized.";
  base::TimeDelta duration = clock_->NowTicks() - start_time_;
  base::UmaHistogramLongTimes(metric_name, duration);
}
