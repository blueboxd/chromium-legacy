// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SUPERVISED_USER_SUPERVISED_USER_EXTENSIONS_METRICS_RECORDER_H_
#define CHROME_BROWSER_SUPERVISED_USER_SUPERVISED_USER_EXTENSIONS_METRICS_RECORDER_H_

#include "chrome/browser/supervised_user/supervised_user_service.h"

// Records UMA metrics for child users using extensions.
// TODO(tobyhuang): Reevaluate if this class should be converted to a namespace
// with a bunch of utility methods instead. If we add more metrics here in the
// future, then we should keep this class.
class SupervisedUserExtensionsMetricsRecorder {
 public:
  // These enum values represent the state that the child user has attained
  // while trying to install an extension.
  // These values are logged to UMA. Entries should not be renumbered and
  // numeric values should never be reused. Please keep in sync with
  // "SupervisedUserExtension" in src/tools/metrics/histograms/enums.xml.
  enum class UmaExtensionState {
    // Recorded when custodian grants child approval to install an extension.
    kNewExtensionApprovalGranted = 0,
    // Recorded when the child approves a new version of an existing extension.
    kNewVersionApprovalGranted = 1,
    // Recorded when the child removes an extension.
    kRemoved = 2,
    // Add future entries above this comment, in sync with
    // "SupervisedUserExtension" in src/tools/metrics/histograms/enums.xml.
    // Update kMaxValue to the last value.
    kMaxValue = kRemoved
  };

  SupervisedUserExtensionsMetricsRecorder() = delete;
  SupervisedUserExtensionsMetricsRecorder(
      const SupervisedUserExtensionsMetricsRecorder&) = delete;
  SupervisedUserExtensionsMetricsRecorder& operator=(
      const SupervisedUserExtensionsMetricsRecorder&) = delete;

  static void RecordExtensionsUmaMetrics(
      SupervisedUserService::ApprovedExtensionChange type);
};

#endif  // CHROME_BROWSER_SUPERVISED_USER_SUPERVISED_USER_EXTENSIONS_METRICS_RECORDER_H_
