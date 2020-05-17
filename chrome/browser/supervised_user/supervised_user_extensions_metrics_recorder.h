// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SUPERVISED_USER_SUPERVISED_USER_EXTENSIONS_METRICS_RECORDER_H_
#define CHROME_BROWSER_SUPERVISED_USER_SUPERVISED_USER_EXTENSIONS_METRICS_RECORDER_H_

#include "base/time/time.h"
#include "chrome/browser/extensions/extension_install_prompt.h"

namespace base {
class TickClock;
}

// Records UMA metrics for supervised users using extensions.
class SupervisedUserExtensionsMetricsRecorder
    : public ExtensionInstallPrompt::Observer {
 public:
  // These enum values represent the state that the child user has attained
  // while trying to install an extension.
  // These values are logged to UMA. Entries should not be renumbered and
  // numeric values should never be reused. Please keep in sync with
  // "SupervisedUserExtension2" in src/tools/metrics/histograms/enums.xml.
  enum class UmaExtensionState {
    // Recorded when custodian grants child approval to install an extension.
    kApprovalGranted = 0,
    // Recorded when the child approves a new version of an existing extension
    // with increased permissions.
    kPermissionsIncreaseGranted = 1,
    // Recorded when the child removes an extension.
    kApprovalRemoved = 2,
    // Add future entries above this comment, in sync with
    // "SupervisedUserExtension2" in src/tools/metrics/histograms/enums.xml.
    // Update kMaxValue to the last value.
    kMaxValue = kApprovalRemoved
  };

  // These enum values represent the state of the Extension Install Dialog for
  // installing and enabling extensions for supervised users.
  // These values are logged to UMA. Entries should not be renumbered and
  // numeric values should never be reused. Please keep in sync with
  // "SupervisedUserExtensionInstallDialog" in
  // src/tools/metrics/histograms/enums.xml.
  enum class ExtensionInstallDialogState {
    // Recorded when the extension install dialog opens.
    kOpened = 0,
    // Recorded when the child clicks "Ask a parent".
    kAskedParent = 1,
    // Recorded when the child cancels the extension installation.
    kChildCanceled = 2,
    // Add future entries above this comment, in sync with
    // "SupervisedUserExtensionInstallDialog" in
    // src/tools/metrics/histograms/enums.xml.
    // Update kMaxValue to the last value.
    kMaxValue = kChildCanceled
  };

  // These enum values represent the state of the Parent Permission Dialog for
  // installing and enabling extensions for supervised users.
  // These values are logged to UMA. Entries should not be renumbered and
  // numeric values should never be reused. Please keep in sync with
  // "SupervisedUserParentPermissionDialog" in
  // src/tools/metrics/histograms/enums.xml.
  enum class ParentPermissionDialogState {
    // Recorded when the parent permission dialog opens.
    kOpened = 0,
    // Recorded when the parent enters their password and successfully approves
    // the extension install.
    kParentApproved = 1,
    // Recorded when the parent cancels the extension installation, denying the
    // supervised user's attempt.
    kParentCanceled = 2,
    // Recorded when there was some sort of failure in the Parent Permission
    // Dialog.
    kFailed = 3,
    // Recorded when the supervised user has no parents, an error. Note that
    // this error triggers the kFailed metric as well.
    kNoParentError = 4,
    // Add future entries above this comment, in sync with
    // "SupervisedUserParentPermissionDialog" in
    // src/tools/metrics/histograms/enums.xml.
    // Update kMaxValue to the last value.
    kMaxValue = kNoParentError
  };

  // These enum values represent supervised user actions to enable or disable an
  // extension.
  // These values are logged to UMA. Entries should not be renumbered and
  // numeric values should never be reused. Please keep in sync with
  // "SupervisedUserExtensionEnablement" in
  // src/tools/metrics/histograms/enums.xml.
  enum class EnablementState {
    // Recorded when the child enables an approved extension.
    kEnabled = 0,
    // Recorded when the child disables an approved extension.
    kDisabled = 1,
    // Add future entries above this comment, in sync with
    // "SupervisedUserExtensionEnablement" in
    // src/tools/metrics/histograms/enums.xml.
    // Update kMaxValue to the last value.
    kMaxValue = kDisabled
  };

  // UMA metrics for adding to or removing from the set of approved extension
  // ids in the kSupervisedUserApprovedExtensions synced pref.
  static const char kExtensionsHistogramName[];
  static const char kApprovalGrantedActionName[];
  static const char kPermissionsIncreaseGrantedActionName[];
  static const char kApprovalRemovedActionName[];

  // UMA metrics for the Extension Install Dialog.
  static const char kExtensionInstallDialogHistogramName[];
  static const char kExtensionInstallDialogAskedParentTimeHistogramName[];
  static const char kExtensionInstallDialogChildCanceledTimeHistogramName[];
  static const char kExtensionInstallDialogOpenedActionName[];
  static const char kExtensionInstallDialogAskedParentActionName[];
  static const char kExtensionInstallDialogChildCanceledActionName[];

  // UMA metrics for the Parent Permission Dialog.
  static const char kParentPermissionDialogHistogramName[];
  static const char kParentPermissionDialogParentApprovedTimeHistogramName[];
  static const char kParentPermissionDialogParentCanceledTimeHistogramName[];
  static const char kParentPermissionDialogFailedTimeHistogramName[];
  static const char kParentPermissionDialogOpenedActionName[];
  static const char kParentPermissionDialogParentApprovedActionName[];
  static const char kParentPermissionDialogParentCanceledActionName[];

  // UMA metrics for enabling or disabling extensions.
  static const char kEnablementHistogramName[];
  static const char kEnabledActionName[];
  static const char kDisabledActionName[];

  SupervisedUserExtensionsMetricsRecorder();
  ~SupervisedUserExtensionsMetricsRecorder() override = default;
  SupervisedUserExtensionsMetricsRecorder(
      const SupervisedUserExtensionsMetricsRecorder&) = delete;
  SupervisedUserExtensionsMetricsRecorder& operator=(
      const SupervisedUserExtensionsMetricsRecorder&) = delete;

  // ExtensionInstallPrompt::Observer:
  void OnDialogOpened() override;
  void OnDialogAccepted() override;
  void OnDialogCanceled() override;

  // Record UMA metrics related to adding or removing extension approvals.
  static void RecordExtensionsUmaMetrics(UmaExtensionState state);

  // Record UMA metrics related to the Extension Install Dialog.
  void RecordExtensionInstallDialogUmaMetrics(
      ExtensionInstallDialogState state);

  // Record UMA metrics related to the Parent Permission Dialog.
  void RecordParentPermissionDialogUmaMetrics(
      ParentPermissionDialogState state);

  // Records when the supervised user enables or disables an approved extension.
  static void RecordEnablementUmaMetrics(EnablementState state);

  // Set clock used for timing for manipulation during tests.
  void SetClockForTesting(const base::TickClock* tick_clock);

 private:
  // Records UMA metric of how long the user spends in the Extension Install or
  // Parent Permission Dialogs in milliseconds.
  void RecordUserTime(const std::string& metric_name) const;

  // Points to the base::DefaultTickClock by default.
  const base::TickClock* clock_;

  // Records when the user initiates the Extension Install or Parent Permission
  // Dialogs.
  base::TimeTicks start_time_;
};

#endif  // CHROME_BROWSER_SUPERVISED_USER_SUPERVISED_USER_EXTENSIONS_METRICS_RECORDER_H_
