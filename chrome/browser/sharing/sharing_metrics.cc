// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/sharing/sharing_metrics.h"

#include "base/metrics/histogram_functions.h"
#include "base/strings/strcat.h"
#include "chrome/browser/sharing/sharing_device_registration_result.h"
#include "components/cast_channel/enum_table.h"
#include "components/ukm/content/source_url_recorder.h"
#include "content/public/browser/web_contents.h"
#include "services/metrics/public/cpp/ukm_builders.h"
#include "services/metrics/public/cpp/ukm_recorder.h"
#include "services/metrics/public/cpp/ukm_source_id.h"

namespace {

// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused.
enum class SharingMessageType {
  kUnknownMessage = 0,
  kPingMessage = 1,
  kAckMessage = 2,
  kClickToCallMessage = 3,
  kSharedClipboardMessage = 4,
  kMaxValue = kSharedClipboardMessage,
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
    case chrome_browser_sharing::SharingMessage::kSharedClipboardMessage:
      return SharingMessageType::kSharedClipboardMessage;
  }
}

const char* GetEnumStringValue(SharingFeatureName feature) {
  DCHECK(feature != SharingFeatureName::kUnknown)
      << "Feature needs to be specified for metrics logging.";

  switch (feature) {
    case SharingFeatureName::kUnknown:
      return "Unknown";
    case SharingFeatureName::kClickToCall:
      return "ClickToCall";
    case SharingFeatureName::kSharedClipboard:
      return "SharedClipboard";
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

void LogSharingDevicesToShow(SharingFeatureName feature,
                             const char* histogram_suffix,
                             int count) {
  auto* feature_str = GetEnumStringValue(feature);
  // Explicitly log both the base and the suffixed histogram because the base
  // aggregation is not automatically generated.
  base::UmaHistogramExactLinear(
      base::StrCat({"Sharing.", feature_str, "DevicesToShow"}), count,
      /*value_max=*/20);
  base::UmaHistogramExactLinear(
      base::StrCat(
          {"Sharing.", feature_str, "DevicesToShow.", histogram_suffix}),
      count,
      /*value_max=*/20);
}

void LogSharingAppsToShow(SharingFeatureName feature,
                          const char* histogram_suffix,
                          int count) {
  auto* feature_str = GetEnumStringValue(feature);
  // Explicitly log both the base and the suffixed histogram because the base
  // aggregation is not automatically generated.
  base::UmaHistogramExactLinear(
      base::StrCat({"Sharing.", feature_str, "AppsToShow"}), count,
      /*value_max=*/20);
  base::UmaHistogramExactLinear(
      base::StrCat({"Sharing.", feature_str, "AppsToShow.", histogram_suffix}),
      count,
      /*value_max=*/20);
}

void LogSharingSelectedDeviceIndex(SharingFeatureName feature,
                                   const char* histogram_suffix,
                                   int index) {
  auto* feature_str = GetEnumStringValue(feature);
  // Explicitly log both the base and the suffixed histogram because the base
  // aggregation is not automatically generated.
  base::UmaHistogramExactLinear(
      base::StrCat({"Sharing.", feature_str, "SelectedDeviceIndex"}), index,
      /*value_max=*/20);
  base::UmaHistogramExactLinear(
      base::StrCat(
          {"Sharing.", feature_str, "SelectedDeviceIndex.", histogram_suffix}),
      index,
      /*value_max=*/20);
}

void LogSharingSelectedAppIndex(SharingFeatureName feature,
                                const char* histogram_suffix,
                                int index) {
  auto* feature_str = GetEnumStringValue(feature);
  // Explicitly log both the base and the suffixed histogram because the base
  // aggregation is not automatically generated.
  base::UmaHistogramExactLinear(
      base::StrCat({"Sharing.", feature_str, "SelectedAppIndex"}), index,
      /*value_max=*/20);
  base::UmaHistogramExactLinear(
      base::StrCat(
          {"Sharing.", feature_str, "SelectedAppIndex.", histogram_suffix}),
      index,
      /*value_max=*/20);
}

void LogSharingMessageAckTime(base::TimeDelta time) {
  base::UmaHistogramMediumTimes("Sharing.MessageAckTime", time);
}

void LogSharingDialogShown(SharingFeatureName feature, SharingDialogType type) {
  base::UmaHistogramEnumeration(
      base::StrCat({"Sharing.", GetEnumStringValue(feature), "DialogShown"}),
      type);
}

void LogSendSharingMessageResult(SharingSendMessageResult result) {
  base::UmaHistogramEnumeration("Sharing.SendMessageResult", result);
}

void LogSendSharingAckMessageResult(SharingSendMessageResult result) {
  base::UmaHistogramEnumeration("Sharing.SendAckMessageResult", result);
}

void LogClickToCallUKM(content::WebContents* web_contents,
                       SharingClickToCallEntryPoint entry_point,
                       bool has_devices,
                       bool has_apps,
                       SharingClickToCallSelection selection) {
  ukm::UkmRecorder* ukm_recorder = ukm::UkmRecorder::Get();
  if (!ukm_recorder)
    return;

  ukm::SourceId source_id =
      ukm::GetSourceIdForWebContentsDocument(web_contents);
  if (source_id == ukm::kInvalidSourceId)
    return;

  ukm::builders::Sharing_ClickToCall(source_id)
      .SetEntryPoint(static_cast<int64_t>(entry_point))
      .SetHasDevices(has_devices)
      .SetHasApps(has_apps)
      .SetSelection(static_cast<int64_t>(selection))
      .Record(ukm_recorder);
}
