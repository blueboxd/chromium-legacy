// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/sharing/sharing_metrics.h"

#include <string.h>

#include "base/metrics/histogram_functions.h"
#include "base/metrics/histogram_macros.h"
#include "base/strings/strcat.h"
#include "chrome/browser/sharing/sharing_device_registration_result.h"
#include "components/cast_channel/enum_table.h"
#include "components/ukm/content/source_url_recorder.h"
#include "content/public/browser/web_contents.h"
#include "services/metrics/public/cpp/ukm_builders.h"
#include "services/metrics/public/cpp/ukm_recorder.h"
#include "services/metrics/public/cpp/ukm_source_id.h"

namespace {
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

const std::string& MessageTypeToMessageSuffix(
    chrome_browser_sharing::MessageType message_type) {
  // For proto3 enums unrecognized enum values are kept when parsing and their
  // name is an empty string. We don't want to use that as a histogram suffix.
  // The returned values must match the values of the SharingMessage suffixes
  // defined in histograms.xml.
  if (!chrome_browser_sharing::MessageType_IsValid(message_type)) {
    return chrome_browser_sharing::MessageType_Name(
        chrome_browser_sharing::UNKNOWN_MESSAGE);
  }
  return chrome_browser_sharing::MessageType_Name(message_type);
}
}  // namespace

chrome_browser_sharing::MessageType SharingPayloadCaseToMessageType(
    chrome_browser_sharing::SharingMessage::PayloadCase payload_case) {
  switch (payload_case) {
    case chrome_browser_sharing::SharingMessage::PAYLOAD_NOT_SET:
      return chrome_browser_sharing::UNKNOWN_MESSAGE;
    case chrome_browser_sharing::SharingMessage::kPingMessage:
      return chrome_browser_sharing::PING_MESSAGE;
    case chrome_browser_sharing::SharingMessage::kAckMessage:
      return chrome_browser_sharing::ACK_MESSAGE;
    case chrome_browser_sharing::SharingMessage::kClickToCallMessage:
      return chrome_browser_sharing::CLICK_TO_CALL_MESSAGE;
    case chrome_browser_sharing::SharingMessage::kSharedClipboardMessage:
      return chrome_browser_sharing::SHARED_CLIPBOARD_MESSAGE;
    case chrome_browser_sharing::SharingMessage::kSmsFetchRequest:
      return chrome_browser_sharing::SMS_FETCH_REQUEST;
  }
  // For proto3 enums unrecognized enum values are kept when parsing, and a new
  // payload case received over the network would not default to
  // PAYLOAD_NOT_SET. Explicitly return UNKNOWN_MESSAGE here to handle this
  // case.
  return chrome_browser_sharing::UNKNOWN_MESSAGE;
}

void LogSharingMessageReceived(
    chrome_browser_sharing::MessageType original_message_type,
    chrome_browser_sharing::SharingMessage::PayloadCase payload_case) {
  chrome_browser_sharing::MessageType actual_message_type =
      SharingPayloadCaseToMessageType(payload_case);
  base::UmaHistogramExactLinear("Sharing.MessageReceivedType",
                                actual_message_type,
                                chrome_browser_sharing::MessageType_ARRAYSIZE);
  base::UmaHistogramExactLinear(
      base::StrCat({"Sharing.MessageReceivedType.",
                    MessageTypeToMessageSuffix(original_message_type)}),
      actual_message_type, chrome_browser_sharing::MessageType_ARRAYSIZE);
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
  if (!histogram_suffix)
    return;
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
  if (!histogram_suffix)
    return;
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
  if (!histogram_suffix)
    return;
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
  if (!histogram_suffix)
    return;
  base::UmaHistogramExactLinear(
      base::StrCat(
          {"Sharing.", feature_str, "SelectedAppIndex.", histogram_suffix}),
      index,
      /*value_max=*/20);
}

void LogSharingMessageAckTime(chrome_browser_sharing::MessageType message_type,
                              base::TimeDelta time) {
  base::UmaHistogramMediumTimes("Sharing.MessageAckTime", time);
  base::UmaHistogramMediumTimes(
      base::StrCat({"Sharing.MessageAckTime.",
                    MessageTypeToMessageSuffix(message_type)}),
      time);
}

void LogSharingDialogShown(SharingFeatureName feature, SharingDialogType type) {
  base::UmaHistogramEnumeration(
      base::StrCat({"Sharing.", GetEnumStringValue(feature), "DialogShown"}),
      type);
}

void LogClickToCallHelpTextClicked(SharingDialogType type) {
  base::UmaHistogramEnumeration("Sharing.ClickToCallHelpTextClicked", type);
}

void LogSendSharingMessageResult(
    chrome_browser_sharing::MessageType message_type,
    SharingSendMessageResult result) {
  base::UmaHistogramEnumeration("Sharing.SendMessageResult", result);
  base::UmaHistogramEnumeration(
      base::StrCat({"Sharing.SendMessageResult.",
                    MessageTypeToMessageSuffix(message_type)}),
      result);
}

void LogSendSharingAckMessageResult(
    chrome_browser_sharing::MessageType message_type,
    SharingSendMessageResult result) {
  base::UmaHistogramEnumeration("Sharing.SendAckMessageResult", result);
  base::UmaHistogramEnumeration(
      base::StrCat({"Sharing.SendAckMessageResult.",
                    MessageTypeToMessageSuffix(message_type)}),
      result);
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

void LogSharedClipboardSelectedTextSize(int text_size) {
  UMA_HISTOGRAM_COUNTS_100000("Sharing.SharedClipboardSelectedTextSize",
                              text_size);
}
