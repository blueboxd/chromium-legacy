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
// picking a device to start a phone call on.
void LogClickToCallDevicesToShow(int count);

// Logs the number of available apps that are about to be shown in a UI for
// picking an app to start a phone call with.
void LogClickToCallAppsToShow(int count);

// Logs the |index| of the device selected by the user for Click to Call. The
// |histogram_suffix| indicates in which UI this event happened and must match
// one from SharingClickToCallUi defined in histograms.xml.
void LogClickToCallSelectedDeviceIndex(const char* histogram_suffix, int index);

// Logs the |index| of the app selected by the user for Click to Call. The
// |histogram_suffix| indicates in which UI this event happened and must match
// one from SharingClickToCallUi defined in histograms.xml.
void LogClickToCallSelectedAppIndex(const char* histogram_suffix, int index);

// Logs to UMA the time from sending a FCM message from the Sharing service
// until an ack message is received for it.
void LogSharingMessageAckTime(base::TimeDelta time);

#endif  // CHROME_BROWSER_SHARING_SHARING_METRICS_H_
