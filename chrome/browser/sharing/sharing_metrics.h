// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SHARING_SHARING_METRICS_H_
#define CHROME_BROWSER_SHARING_SHARING_METRICS_H_

#include "base/time/time.h"
#include "chrome/browser/sharing/shared_clipboard/remote_copy_handle_message_result.h"
#include "chrome/browser/sharing/sharing_constants.h"
#include "chrome/browser/sharing/sharing_send_message_result.h"
#include "components/sync/protocol/sharing_message.pb.h"

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

chrome_browser_sharing::MessageType SharingPayloadCaseToMessageType(
    chrome_browser_sharing::SharingMessage::PayloadCase payload_case);

// Logs the |payload_case| to UMA. This should be called when a SharingMessage
// is received. Additionally, a suffixed version of the histogram is logged
// using |original_message_type| which is different from the actual message type
// for ack messages.
void LogSharingMessageReceived(
    chrome_browser_sharing::MessageType original_message_type,
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
void LogSharingMessageAckTime(chrome_browser_sharing::MessageType message_type,
                              base::TimeDelta time);

// Logs to UMA the number of hours since the target device timestamp was last
// updated. Logged when a message is sent to the device.
void LogSharingDeviceLastUpdatedAge(
    chrome_browser_sharing::MessageType message_type,
    base::TimeDelta age);

// Logs to UMA the comparison of the major version of Chrome on this
// (the sender) device and the receiver device. Logged when a message is sent.
// The |receiver_version| should be a dotted version number with optional
// modifiers e.g. "1.2.3.4 canary" as generated by
// version_info::GetVersionStringWithModifier.
void LogSharingVersionComparison(
    chrome_browser_sharing::MessageType message_type,
    const std::string& receiver_version);

// Logs to UMA the |type| of dialog shown for sharing feature.
void LogSharingDialogShown(SharingFeatureName feature, SharingDialogType type);

// Logs to UMA result of sending a SharingMessage. This should not be called for
// sending ack messages.
void LogSendSharingMessageResult(
    chrome_browser_sharing::MessageType message_type,
    SharingDevicePlatform receiver_device_platform,
    SharingSendMessageResult result);

// Logs to UMA result of sendin an ack of a SharingMessage.
void LogSendSharingAckMessageResult(
    chrome_browser_sharing::MessageType message_type,
    SharingDevicePlatform ack_receiver_device_type,
    SharingSendMessageResult result);

// Logs to UMA the size of the selected text for Shared Clipboard.
void LogSharedClipboardSelectedTextSize(size_t text_size);

// Logs to UMA the result of handling a Remote Copy message.
void LogRemoteCopyHandleMessageResult(RemoteCopyHandleMessageResult result);

// Logs to UMA the size of the received text for Remote Copy.
void LogRemoteCopyReceivedTextSize(size_t size);

// Logs to UMA the size of the received image (before decoding) for Remote Copy.
void LogRemoteCopyReceivedImageSizeBeforeDecode(size_t size);

// Logs to UMA the size of the received image (after decoding) for Remote Copy.
void LogRemoteCopyReceivedImageSizeAfterDecode(size_t size);

// Logs to UMA the status code of an image load request for Remote Copy.
void LogRemoteCopyLoadImageStatusCode(int code);

// Logs to UMA the time to load an image for Remote Copy.
void LogRemoteCopyLoadImageTime(base::TimeDelta time);

// Logs to UMA the time to decode an image for Remote Copy.
void LogRemoteCopyDecodeImageTime(base::TimeDelta time);

// Logs to UMA the time to resize an image for Remote Copy.
void LogRemoteCopyResizeImageTime(base::TimeDelta time);

#endif  // CHROME_BROWSER_SHARING_SHARING_METRICS_H_
