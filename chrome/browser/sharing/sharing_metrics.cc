// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/sharing/sharing_metrics.h"

#include "base/metrics/histogram_functions.h"
#include "base/strings/strcat.h"
#include "base/strings/string_number_conversions.h"
#include "chrome/browser/sharing/sharing_device_registration_result.h"
#include "components/version_info/version_info.h"

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

// These value are mapped to histogram suffixes. Please keep in sync with
// "SharingDevicePlatform" in src/tools/metrics/histograms/enums.xml.
std::string DevicePlatformToString(SharingDevicePlatform device_platform) {
  switch (device_platform) {
    case SharingDevicePlatform::kAndroid:
      return "Android";
    case SharingDevicePlatform::kChromeOS:
      return "ChromeOS";
    case SharingDevicePlatform::kIOS:
      return "iOS";
    case SharingDevicePlatform::kLinux:
      return "Linux";
    case SharingDevicePlatform::kMac:
      return "Mac";
    case SharingDevicePlatform::kWindows:
      return "Windows";
    case SharingDevicePlatform::kUnknown:
      return "Unknown";
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

// Major Chrome version comparison with the receiver device.
// These values are logged to UMA. Entries should not be renumbered and numeric
// values should never be reused. Please keep in sync with
// "SharingMajorVersionComparison" in enums.xml.
enum class SharingMajorVersionComparison {
  kUnknown = 0,
  kSenderIsLower = 1,
  kSame = 2,
  kSenderIsHigher = 3,
  kMaxValue = kSenderIsHigher,
};
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
    case chrome_browser_sharing::SharingMessage::kRemoteCopyMessage:
      return chrome_browser_sharing::REMOTE_COPY_MESSAGE;
    case chrome_browser_sharing::SharingMessage::kSignallingMessage:
      return chrome_browser_sharing::SIGNALLING_MESSAGE;
    case chrome_browser_sharing::SharingMessage::kIceCandidateMessage:
      return chrome_browser_sharing::ICE_CANDIDATE_MESSAGE;
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
  std::string suffixed_name = base::StrCat(
      {"Sharing.MessageAckTime.", MessageTypeToMessageSuffix(message_type)});
  switch (message_type) {
    case chrome_browser_sharing::MessageType::UNKNOWN_MESSAGE:
    case chrome_browser_sharing::MessageType::PING_MESSAGE:
    case chrome_browser_sharing::MessageType::CLICK_TO_CALL_MESSAGE:
    case chrome_browser_sharing::MessageType::SHARED_CLIPBOARD_MESSAGE:
      base::UmaHistogramMediumTimes(suffixed_name, time);
      break;
    case chrome_browser_sharing::MessageType::SMS_FETCH_REQUEST:
      base::UmaHistogramCustomTimes(
          suffixed_name, time, /*min=*/base::TimeDelta::FromMilliseconds(1),
          /*max=*/base::TimeDelta::FromMinutes(10), /*buckets=*/50);
      break;
    case chrome_browser_sharing::MessageType::ACK_MESSAGE:
    default:
      // For proto3 enums unrecognized enum values are kept, so message_type may
      // not fall into any switch case. However, as an ack message, original
      // message type should always be known.
      NOTREACHED();
  }
}

void LogSharingDeviceLastUpdatedAge(
    chrome_browser_sharing::MessageType message_type,
    base::TimeDelta age) {
  constexpr char kBase[] = "Sharing.DeviceLastUpdatedAge";
  int hours = age.InHours();
  base::UmaHistogramCounts1000(kBase, hours);
  base::UmaHistogramCounts1000(
      base::StrCat({kBase, ".", MessageTypeToMessageSuffix(message_type)}),
      hours);
}

void LogSharingVersionComparison(
    chrome_browser_sharing::MessageType message_type,
    const std::string& receiver_version) {
  int sender_major = 0;
  base::StringToInt(version_info::GetMajorVersionNumber(), &sender_major);

  // The |receiver_version| has optional modifiers e.g. "1.2.3.4 canary" so we
  // do not parse it with base::Version.
  int receiver_major = 0;
  base::StringToInt(receiver_version, &receiver_major);

  SharingMajorVersionComparison result;
  if (sender_major == 0 || sender_major == INT_MIN || sender_major == INT_MAX ||
      receiver_major == 0 || receiver_major == INT_MIN ||
      receiver_major == INT_MAX) {
    result = SharingMajorVersionComparison::kUnknown;
  } else if (sender_major < receiver_major) {
    result = SharingMajorVersionComparison::kSenderIsLower;
  } else if (sender_major == receiver_major) {
    result = SharingMajorVersionComparison::kSame;
  } else {
    result = SharingMajorVersionComparison::kSenderIsHigher;
  }
  constexpr char kBase[] = "Sharing.MajorVersionComparison";
  base::UmaHistogramEnumeration(kBase, result);
  base::UmaHistogramEnumeration(
      base::StrCat({kBase, ".", MessageTypeToMessageSuffix(message_type)}),
      result);
}

void LogSharingDialogShown(SharingFeatureName feature, SharingDialogType type) {
  base::UmaHistogramEnumeration(
      base::StrCat({"Sharing.", GetEnumStringValue(feature), "DialogShown"}),
      type);
}

void LogSendSharingMessageResult(
    chrome_browser_sharing::MessageType message_type,
    SharingDevicePlatform receiving_device_platform,
    SharingSendMessageResult result) {
  const std::string metric_prefix = "Sharing.SendMessageResult";

  base::UmaHistogramEnumeration(metric_prefix, result);
  base::UmaHistogramEnumeration(
      base::StrCat(
          {metric_prefix, ".", MessageTypeToMessageSuffix(message_type)}),
      result);

  base::UmaHistogramEnumeration(
      base::StrCat({metric_prefix, ".",
                    DevicePlatformToString(receiving_device_platform)}),
      result);
  base::UmaHistogramEnumeration(
      base::StrCat({metric_prefix, ".",
                    DevicePlatformToString(receiving_device_platform), ".",
                    MessageTypeToMessageSuffix(message_type)}),
      result);
}

void LogSendSharingAckMessageResult(
    chrome_browser_sharing::MessageType message_type,
    SharingDevicePlatform ack_receiver_device_type,
    SharingSendMessageResult result) {
  const std::string metric_prefix = "Sharing.SendAckMessageResult";

  base::UmaHistogramEnumeration(metric_prefix, result);
  base::UmaHistogramEnumeration(
      base::StrCat(
          {metric_prefix, ".", MessageTypeToMessageSuffix(message_type)}),
      result);

  base::UmaHistogramEnumeration(
      base::StrCat({metric_prefix, ".",
                    DevicePlatformToString(ack_receiver_device_type)}),
      result);
  base::UmaHistogramEnumeration(
      base::StrCat({metric_prefix, ".",
                    DevicePlatformToString(ack_receiver_device_type), ".",
                    MessageTypeToMessageSuffix(message_type)}),
      result);
}

void LogSharedClipboardSelectedTextSize(size_t size) {
  base::UmaHistogramCounts100000("Sharing.SharedClipboardSelectedTextSize",
                                 size);
}

void LogRemoteCopyHandleMessageResult(RemoteCopyHandleMessageResult result) {
  base::UmaHistogramEnumeration("Sharing.RemoteCopyHandleMessageResult",
                                result);
}

void LogRemoteCopyReceivedTextSize(size_t size) {
  base::UmaHistogramCounts100000("Sharing.RemoteCopyReceivedTextSize", size);
}

void LogRemoteCopyReceivedImageSizeBeforeDecode(size_t size) {
  base::UmaHistogramCounts10M("Sharing.RemoteCopyReceivedImageSizeBeforeDecode",
                              size);
}

void LogRemoteCopyReceivedImageSizeAfterDecode(size_t size) {
  base::UmaHistogramCustomCounts(
      "Sharing.RemoteCopyReceivedImageSizeAfterDecode", size, 1, 100000000, 50);
}

void LogRemoteCopyLoadImageStatusCode(int code) {
  base::UmaHistogramSparse("Sharing.RemoteCopyLoadImageStatusCode", code);
}

void LogRemoteCopyLoadImageTime(base::TimeDelta time) {
  base::UmaHistogramMediumTimes("Sharing.RemoteCopyLoadImageTime", time);
}

void LogRemoteCopyDecodeImageTime(base::TimeDelta time) {
  base::UmaHistogramMediumTimes("Sharing.RemoteCopyDecodeImageTime", time);
}

void LogRemoteCopyResizeImageTime(base::TimeDelta time) {
  base::UmaHistogramMediumTimes("Sharing.RemoteCopyResizeImageTime", time);
}
