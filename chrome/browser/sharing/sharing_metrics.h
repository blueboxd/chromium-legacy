// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SHARING_SHARING_METRICS_H_
#define CHROME_BROWSER_SHARING_SHARING_METRICS_H_

#include "chrome/browser/sharing/proto/sharing_message.pb.h"

#include "base/time/time.h"
#include "chrome/browser/sharing/sharing_constants.h"
#include "chrome/browser/sharing/sharing_send_message_result.h"

namespace content {
class WebContents;
}  // namespace content

enum class SharingDeviceRegistrationResult;

// Result of VAPID key creation during Sharing registration.
// These values are logged to UMA. Entries should not be renumbered and numeric
// values should never be reused. Please keep in sync with
// "SharingVapidKeyCreationResult" in src/tools/metrics/histograms/enums.xml.
enum class SharingVapidKeyCreationResult {
  kSuccess = 0,
  kGenerateECKeyFailed = 1,
  kExportPrivateKeyFailed = 2,
  kMaxValue = kExportPrivateKeyFailed,
};

// The types of dialogs that can be shown for sharing features.
// These values are logged to UMA. Entries should not be renumbered and numeric
// values should never be reused. Please keep in sync with
// "SharingDialogType" in src/tools/metrics/histograms/enums.xml.
// GENERATED_JAVA_ENUM_PACKAGE: org.chromium.chrome.browser.sharing
enum class SharingDialogType {
  kDialogWithDevicesMaybeApps = 0,
  kDialogWithoutDevicesWithApp = 1,
  kEducationalDialog = 2,
  kErrorDialog = 3,
  kMaxValue = kErrorDialog,
};

// These histogram suffixes must match the ones in Sharing{feature}Ui
// defined in histograms.xml.
const char kSharingUiContextMenu[] = "ContextMenu";
const char kSharingUiDialog[] = "Dialog";
// Entry point of a Click to Call journey.
// These values are logged to UKM. Entries should not be renumbered and numeric
// values should never be reused. Please keep in sync with
// "SharingClickToCallEntryPoint" in src/tools/metrics/histograms/enums.xml.
enum class SharingClickToCallEntryPoint {
  kLeftClickLink = 0,
  kRightClickLink = 1,
  kRightClickSelection = 2,
  kMaxValue = kRightClickSelection,
};

// Selection at the end of a Click to Call journey.
// These values are logged to UKM. Entries should not be renumbered and numeric
// values should never be reused. Please keep in sync with
// "SharingClickToCallSelection" in src/tools/metrics/histograms/enums.xml.
enum class SharingClickToCallSelection {
  kNone = 0,
  kDevice = 1,
  kApp = 2,
  kMaxValue = kApp,
};

// These histogram suffixes must match the ones in SharingClickToCallUi defined
// in histograms.xml.
const char kSharingClickToCallUiContextMenu[] = "ContextMenu";
const char kSharingClickToCallUiDialog[] = "Dialog";

// Logs the |payload_case| to UMA. This should be called when a SharingMessage
// is received.
void LogSharingMessageReceived(
    chrome_browser_sharing::SharingMessage::PayloadCase payload_case);

// Logs the |result| to UMA. This should be called after attempting register
// Sharing.
void LogSharingRegistrationResult(SharingDeviceRegistrationResult result);

// Logs the |result| to UMA. This should be called after attempting un-register
// Sharing.
void LogSharingUnegistrationResult(SharingDeviceRegistrationResult result);

// Logs the |result| to UMA. This should be called after attempting to create
// VAPID keys.
void LogSharingVapidKeyCreationResult(SharingVapidKeyCreationResult result);

// Logs the number of available devices that are about to be shown in a UI for
// picking a device to start a sharing functionality. The |histogram_suffix|
// indicates in which UI this event happened and must match one from
// Sharing{feature}Ui defined in histograms.xml use the constants defined
// in this file for that.
// TODO(yasmo): Change histogram_suffix to be an enum type.
void LogSharingDevicesToShow(SharingFeatureName feature,
                             const char* histogram_suffix,
                             int count);

// Logs the number of available apps that are about to be shown in a UI for
// picking an app to start a sharing functionality. The |histogram_suffix|
// indicates in which UI this event happened and must match one from
// Sharing{feature}Ui defined in histograms.xml - use the constants defined
// in this file for that.
void LogSharingAppsToShow(SharingFeatureName feature,
                          const char* histogram_suffix,
                          int count);

// Logs the |index| of the device selected by the user for sharing feature. The
// |histogram_suffix| indicates in which UI this event happened and must match
// one from Sharing{feature}Ui defined in histograms.xml - use the
// constants defined in this file for that.
void LogSharingSelectedDeviceIndex(SharingFeatureName feature,
                                   const char* histogram_suffix,
                                   int index);

// Logs the |index| of the app selected by the user for sharing feature. The
// |histogram_suffix| indicates in which UI this event happened and must match
// one from Sharing{feature}Ui defined in histograms.xml - use the
// constants defined in this file for that.
void LogSharingSelectedAppIndex(SharingFeatureName feature,
                                const char* histogram_suffix,
                                int index);

// Logs to UMA the time from sending a FCM message from the Sharing service
// until an ack message is received for it.
void LogSharingMessageAckTime(base::TimeDelta time);

// Logs to UMA the |type| of dialog shown for sharing feature.
void LogSharingDialogShown(SharingFeatureName feature, SharingDialogType type);

// Logs the dialog type when a user clicks on the help text in the Click to Call
// dialog.
void LogClickToCallHelpTextClicked(SharingDialogType type);

// Logs to UMA result of sending a SharingMessage. This should not be called for
// sending ack messages.
void LogSendSharingMessageResult(SharingSendMessageResult result);

// Logs to UMA result of sendin an ack of a SharingMessage.
void LogSendSharingAckMessageResult(SharingSendMessageResult result);

// Records a Click to Call selection to UKM. This is logged after a completed
// action like selecting an app or a device to send the phone number to.
void LogClickToCallUKM(content::WebContents* web_contents,
                       SharingClickToCallEntryPoint entry_point,
                       bool has_devices,
                       bool has_apps,
                       SharingClickToCallSelection selection);

// Records the size of the selected text in Shared Clipboard.
void LogSharedClipboardSelectedTextSize(int text_size);

#endif  // CHROME_BROWSER_SHARING_SHARING_METRICS_H_
