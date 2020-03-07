// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/supervised_user/supervised_user_extensions_metrics_recorder.h"

#include "base/logging.h"
#include "base/metrics/histogram_functions.h"
#include "base/metrics/user_metrics.h"

namespace {
constexpr char kHistogramName[] = "SupervisedUsers.Extensions";
constexpr char kNewExtensionApprovalGrantedActionName[] =
    "SupervisedUsers_Extensions_NewExtensionApprovalGranted";
constexpr char kNewVersionApprovalGrantedActionName[] =
    "SupervisedUsers_Extensions_NewVersionApprovalGranted";
constexpr char kRemovedActionName[] = "SupervisedUsers_Extensions_Removed";
}  // namespace

// static
void SupervisedUserExtensionsMetricsRecorder::RecordExtensionsUmaMetrics(
    syncer::SyncChange::SyncChangeType type) {
  switch (type) {
    case syncer::SyncChange::ACTION_ADD:
      // Record UMA metrics for custodian approval for a new extension.
      base::RecordAction(
          base::UserMetricsAction(kNewExtensionApprovalGrantedActionName));
      base::UmaHistogramEnumeration(
          kHistogramName, UmaExtensionState::kNewExtensionApprovalGranted);
      break;
    case syncer::SyncChange::ACTION_UPDATE:
      // Record UMA metrics for child approval for a newer version of an
      // existing extension.
      base::RecordAction(
          base::UserMetricsAction(kNewVersionApprovalGrantedActionName));
      base::UmaHistogramEnumeration(
          kHistogramName, UmaExtensionState::kNewVersionApprovalGranted);
      break;
    case syncer::SyncChange::ACTION_DELETE:
      // Record UMA metrics for removing an extension.
      base::RecordAction(base::UserMetricsAction(kRemovedActionName));
      base::UmaHistogramEnumeration(kHistogramName,
                                    UmaExtensionState::kRemoved);
      break;
    case syncer::SyncChange::ACTION_INVALID:
      NOTREACHED();
      break;
  }
}
