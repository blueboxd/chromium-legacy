// Copyright (c) 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/safe_browsing/cloud_content_scanning/deep_scanning_utils.h"
#include <algorithm>

#include "base/metrics/histogram_functions.h"
#include "chrome/browser/extensions/api/safe_browsing_private/safe_browsing_private_event_router.h"
#include "chrome/browser/extensions/api/safe_browsing_private/safe_browsing_private_event_router_factory.h"

namespace safe_browsing {

namespace {

constexpr int kMinBytesPerSecond = 1;
constexpr int kMaxBytesPerSecond = 100 * 1024 * 1024;  // 100 MB/s

// TODO(drubery): This function would be simpler if the ClientDownloadResponse
// and MalwareDeepScanningVerdict used the same enum.
std::string MalwareVerdictToThreatType(
    MalwareDeepScanningVerdict::Verdict verdict) {
  switch (verdict) {
    case MalwareDeepScanningVerdict::CLEAN:
      return "SAFE";
    case MalwareDeepScanningVerdict::UWS:
      return "POTENTIALLY_UNWANTED";
    case MalwareDeepScanningVerdict::MALWARE:
      return "DANGEROUS";
    case MalwareDeepScanningVerdict::VERDICT_UNSPECIFIED:
    default:
      return "UNKNOWN";
  }
}

}  // namespace

void MaybeReportDeepScanningVerdict(Profile* profile,
                                    const GURL& url,
                                    const std::string& file_name,
                                    const std::string& download_digest_sha256,
                                    const std::string& mime_type,
                                    const std::string& trigger,
                                    const int64_t content_size,
                                    BinaryUploadService::Result result,
                                    DeepScanningClientResponse response) {
  DCHECK(std::all_of(download_digest_sha256.begin(),
                     download_digest_sha256.end(), [](const char& c) {
                       return (c >= '0' && c <= '9') ||
                              (c >= 'A' && c <= 'F') || (c >= 'a' && c <= 'f');
                     }));
  if (result == BinaryUploadService::Result::FILE_TOO_LARGE) {
    extensions::SafeBrowsingPrivateEventRouterFactory::GetForProfile(profile)
        ->OnUnscannedFileEvent(url, file_name, download_digest_sha256,
                               mime_type, trigger, "fileTooLarge",
                               content_size);
  } else if (result == BinaryUploadService::Result::TIMEOUT) {
    extensions::SafeBrowsingPrivateEventRouterFactory::GetForProfile(profile)
        ->OnUnscannedFileEvent(url, file_name, download_digest_sha256,
                               mime_type, trigger, "scanTimedOut",
                               content_size);
  }

  if (result != BinaryUploadService::Result::SUCCESS)
    return;

  if (response.malware_scan_verdict().verdict() ==
          MalwareDeepScanningVerdict::UWS ||
      response.malware_scan_verdict().verdict() ==
          MalwareDeepScanningVerdict::MALWARE) {
    extensions::SafeBrowsingPrivateEventRouterFactory::GetForProfile(profile)
        ->OnDangerousDeepScanningResult(
            url, file_name, download_digest_sha256,
            MalwareVerdictToThreatType(
                response.malware_scan_verdict().verdict()),
            mime_type, trigger, content_size);
  }

  if (response.dlp_scan_verdict().status() == DlpDeepScanningVerdict::SUCCESS) {
    if (!response.dlp_scan_verdict().triggered_rules().empty()) {
      extensions::SafeBrowsingPrivateEventRouterFactory::GetForProfile(profile)
          ->OnSensitiveDataEvent(response.dlp_scan_verdict(), url, file_name,
                                 download_digest_sha256, mime_type, trigger,
                                 content_size);
    }
  }
}

std::string DeepScanAccessPointToString(DeepScanAccessPoint access_point) {
  // TODO(domfc): Add PASTE access point.
  switch (access_point) {
    case DeepScanAccessPoint::DOWNLOAD:
      return "Download";
    case DeepScanAccessPoint::UPLOAD:
      return "Upload";
    case DeepScanAccessPoint::DRAG_AND_DROP:
      return "DragAndDrop";
  }
  NOTREACHED();
  return "";
}

void RecordDeepScanMetrics(DeepScanAccessPoint access_point,
                           base::TimeDelta duration,
                           int64_t total_bytes,
                           const BinaryUploadService::Result& result,
                           const DeepScanningClientResponse& response) {
  bool dlp_verdict_success = response.has_dlp_scan_verdict()
                                 ? response.dlp_scan_verdict().status() ==
                                       DlpDeepScanningVerdict::SUCCESS
                                 : true;
  bool malware_verdict_success =
      response.has_malware_scan_verdict()
          ? response.malware_scan_verdict().verdict() !=
                MalwareDeepScanningVerdict::VERDICT_UNSPECIFIED
          : true;
  bool success = dlp_verdict_success && malware_verdict_success;
  std::string result_value;
  switch (result) {
    case BinaryUploadService::Result::SUCCESS:
      if (success)
        result_value = "Success";
      else
        result_value = "FailedToGetVerdict";
      break;
    case BinaryUploadService::Result::UPLOAD_FAILURE:
      result_value = "UploadFailure";
      break;
    case BinaryUploadService::Result::TIMEOUT:
      result_value = "Timeout";
      break;
    case BinaryUploadService::Result::FILE_TOO_LARGE:
      result_value = "FileTooLarge";
      break;
    case BinaryUploadService::Result::FAILED_TO_GET_TOKEN:
      result_value = "FailedToGetToken";
      break;
    case BinaryUploadService::Result::UNKNOWN:
      result_value = "Unknown";
      break;
    case BinaryUploadService::Result::UNAUTHORIZED:
      // Don't record UMA metrics for this result.
      return;
  }

  // Update |success| so non-SUCCESS results don't log the bytes/sec metric.
  success &= (result == BinaryUploadService::Result::SUCCESS);

  RecordDeepScanMetrics(access_point, duration, total_bytes, result_value,
                        success);
}

void RecordDeepScanMetrics(DeepScanAccessPoint access_point,
                           base::TimeDelta duration,
                           int64_t total_bytes,
                           const std::string& result,
                           bool success) {
  // Don't record metrics if the duration is unusable.
  if (duration.InMilliseconds() == 0)
    return;

  std::string access_point_string = DeepScanAccessPointToString(access_point);
  if (success) {
    base::UmaHistogramCustomCounts(
        "SafeBrowsing.DeepScan." + access_point_string + ".BytesPerSeconds",
        (1000 * total_bytes) / duration.InMilliseconds(),
        /*min=*/kMinBytesPerSecond,
        /*max=*/kMaxBytesPerSecond,
        /*buckets=*/50);
  }

  // The scanning timeout is 5 minutes, so the bucket maximum time is 30 minutes
  // in order to be lenient and avoid having lots of data in the overlow bucket.
  base::UmaHistogramCustomTimes("SafeBrowsing.DeepScan." + access_point_string +
                                    "." + result + ".Duration",
                                duration, base::TimeDelta::FromMilliseconds(1),
                                base::TimeDelta::FromMinutes(30), 50);
  base::UmaHistogramCustomTimes(
      "SafeBrowsing.DeepScan." + access_point_string + ".Duration", duration,
      base::TimeDelta::FromMilliseconds(1), base::TimeDelta::FromMinutes(30),
      50);
}

}  // namespace safe_browsing
