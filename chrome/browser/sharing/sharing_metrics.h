// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SHARING_SHARING_METRICS_H_
#define CHROME_BROWSER_SHARING_SHARING_METRICS_H_

#include "chrome/browser/sharing/proto/sharing_message.pb.h"

#include "base/time/time.h"

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

// The types of dialogs that can be shown for Click to Call.
// These values are logged to UMA. Entries should not be renumbered and numeric
// values should never be reused. Please keep in sync with
// "SharingClickToCallDialogType" in src/tools/metrics/histograms/enums.xml.
enum class SharingClickToCallDialogType {
  kDialogWithDevicesMaybeApps = 0,
  kDialogWithoutDevicesWithApp = 1,
  kEducationalDialog = 2,
  kErrorDialog = 3,
  kMaxValue = kErrorDialog,
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
// picking a device to start a phone call on. The |histogram_suffix| indicates
// in which UI this event happened and must match one from SharingClickToCallUi
// defined in histograms.xml - use the constants defined in this file for that.
void LogClickToCallDevicesToShow(const char* histogram_suffix, int count);

// Logs the number of available apps that are about to be shown in a UI for
// picking an app to start a phone call with. The |histogram_suffix| indicates
// in which UI this event happened and must match one from SharingClickToCallUi
// defined in histograms.xml - use the constants defined in this file for that.
void LogClickToCallAppsToShow(const char* histogram_suffix, int count);

// Logs the |index| of the device selected by the user for Click to Call. The
// |histogram_suffix| indicates in which UI this event happened and must match
// one from SharingClickToCallUi defined in histograms.xml - use the constants
// defined in this file for that.
void LogClickToCallSelectedDeviceIndex(const char* histogram_suffix, int index);

// Logs the |index| of the app selected by the user for Click to Call. The
// |histogram_suffix| indicates in which UI this event happened and must match
// one from SharingClickToCallUi defined in histograms.xml - use the constants
// defined in this file for that.
void LogClickToCallSelectedAppIndex(const char* histogram_suffix, int index);

// Logs to UMA the time from sending a FCM message from the Sharing service
// until an ack message is received for it.
void LogSharingMessageAckTime(base::TimeDelta time);

// Logs to UMA the |type| of dialog shown for Click to Call.
void LogClickToCallDialogShown(SharingClickToCallDialogType type);

// Logs to UMA whether a SharingMessage was sent successfully by the Sharing
// service. For |success| to be true, an ack message must be received before the
// timeout. This should not be called for sending ack messages.
void LogSendSharingMessageSuccess(bool success);

// Logs to UMA whether an ack message was sent successfully by the Sharing
// service.
void LogSendSharingAckMessageSuccess(bool success);

#endif  // CHROME_BROWSER_SHARING_SHARING_METRICS_H_
