// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/supervised_user/supervised_user_extensions_metrics_recorder.h"

#include "base/metrics/histogram_functions.h"
#include "base/metrics/user_metrics.h"

namespace {
constexpr char kHistogramName[] = "SupervisedUsers.Extensions2";
constexpr char kApprovalGrantedActionName[] =
    "SupervisedUsers_Extensions_ApprovalGranted";
constexpr char kPermissionsIncreaseGrantedActionName[] =
    "SupervisedUsers_Extensions_PermissionsIncreaseGranted";
constexpr char kApprovalRemovedActionName[] =
    "SupervisedUsers_Extensions_ApprovalRemoved";
}  // namespace

// static
void SupervisedUserExtensionsMetricsRecorder::RecordExtensionsUmaMetrics(
    UmaExtensionState state) {
  base::UmaHistogramEnumeration(kHistogramName, state);
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
