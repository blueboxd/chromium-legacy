// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/sharing/sharing_metrics.h"

#include "base/metrics/histogram_functions.h"
#include "base/strings/strcat.h"
#include "chrome/browser/sharing/sharing_device_registration_result.h"

namespace {

// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused.
enum class SharingMessageType {
  kUnknownMessage = 0,
  kPingMessage = 1,
  kAckMessage = 2,
  kClickToCallMessage = 3,
  kMaxValue = kClickToCallMessage,
};

SharingMessageType PayloadCaseToMessageType(
    chrome_browser_sharing::SharingMessage::PayloadCase payload_case) {
  switch (payload_case) {
    case chrome_browser_sharing::SharingMessage::PAYLOAD_NOT_SET:
      return SharingMessageType::kUnknownMessage;
    case chrome_browser_sharing::SharingMessage::kPingMessage:
      return SharingMessageType::kPingMessage;
    case chrome_browser_sharing::SharingMessage::kAckMessage:
      return SharingMessageType::kAckMessage;
    case chrome_browser_sharing::SharingMessage::kClickToCallMessage:
      return SharingMessageType::kClickToCallMessage;
  }
}

}  // namespace

void LogSharingMessageReceived(
    chrome_browser_sharing::SharingMessage::PayloadCase payload_case) {
  base::UmaHistogramEnumeration("Sharing.MessageReceivedType",
                                PayloadCaseToMessageType(payload_case));
}

void LogSharingRegistrationResult(SharingDeviceRegistrationResult result) {
  base::UmaHistogramEnumeration("Sharing.DeviceRegistrationResult", result);
}

void LogSharingUnegistrationResult(SharingDeviceRegistrationResult result) {
  base::UmaHistogramEnumeration("Sharing.DeviceUnregistrationResult", result);
}

void LogSharingVapidKeyCreationResult(SharingVapidKeyCreationResult result) {
  base::UmaHistogramEnumeration("Sharing.VapidKeyCreationResult", result);
}

void LogClickToCallDevicesToShow(const char* histogram_suffix, int count) {
  // Explicitly log both the base and the suffixed histogram because the base
  // aggregation is not automatically generated.
  base::UmaHistogramExactLinear("Sharing.ClickToCallDevicesToShow", count,
                                /*value_max=*/20);
  base::UmaHistogramExactLinear(
      base::StrCat({"Sharing.ClickToCallDevicesToShow.", histogram_suffix}),
      count,
      /*value_max=*/20);
}

void LogClickToCallAppsToShow(const char* histogram_suffix, int count) {
  // Explicitly log both the base and the suffixed histogram because the base
  // aggregation is not automatically generated.
  base::UmaHistogramExactLinear("Sharing.ClickToCallAppsToShow", count,
                                /*value_max=*/20);
  base::UmaHistogramExactLinear(
      base::StrCat({"Sharing.ClickToCallAppsToShow.", histogram_suffix}), count,
      /*value_max=*/20);
}

void LogClickToCallSelectedDeviceIndex(const char* histogram_suffix,
                                       int index) {
  // Explicitly log both the base and the suffixed histogram because the base
  // aggregation is not automatically generated.
  base::UmaHistogramExactLinear("Sharing.ClickToCallSelectedDeviceIndex", index,
                                /*value_max=*/20);
  base::UmaHistogramExactLinear(
      base::StrCat(
          {"Sharing.ClickToCallSelectedDeviceIndex.", histogram_suffix}),
      index,
      /*value_max=*/20);
}

void LogClickToCallSelectedAppIndex(const char* histogram_suffix, int index) {
  // Explicitly log both the base and the suffixed histogram because the base
  // aggregation is not automatically generated.
  base::UmaHistogramExactLinear("Sharing.ClickToCallSelectedAppIndex", index,
                                /*value_max=*/20);
  base::UmaHistogramExactLinear(
      base::StrCat({"Sharing.ClickToCallSelectedAppIndex.", histogram_suffix}),
      index,
      /*value_max=*/20);
}

void LogSharingMessageAckTime(base::TimeDelta time) {
  base::UmaHistogramMediumTimes("Sharing.MessageAckTime", time);
}

void LogClickToCallDialogShown(SharingClickToCallDialogType type) {
  base::UmaHistogramEnumeration("Sharing.ClickToCallDialogShown", type);
}

void LogSendSharingMessageSuccess(bool success) {
  base::UmaHistogramBoolean("Sharing.SendMessageSuccess", success);
}

void LogSendSharingAckMessageSuccess(bool success) {
  base::UmaHistogramBoolean("Sharing.SendAckMessageSuccess", success);
}
