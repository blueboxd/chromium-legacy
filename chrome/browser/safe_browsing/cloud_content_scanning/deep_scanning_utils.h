// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SAFE_BROWSING_CLOUD_CONTENT_SCANNING_DEEP_SCANNING_UTILS_H_
#define CHROME_BROWSER_SAFE_BROWSING_CLOUD_CONTENT_SCANNING_DEEP_SCANNING_UTILS_H_

#include <string>
#include "base/time/time.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/safe_browsing/cloud_content_scanning/binary_upload_service.h"
#include "url/gurl.h"

class Profile;

namespace safe_browsing {
// Helper function to examine a DeepScanningClientResponse and report the
// appropriate events to the enterprise admin. |download_digest_sha256| must be
// encoded using base::HexEncode.
void MaybeReportDeepScanningVerdict(Profile* profile,
                                    const GURL& url,
                                    const std::string& file_name,
                                    const std::string& download_digest_sha256,
                                    const std::string& mime_type,
                                    const std::string& trigger,
                                    const int64_t content_size,
                                    BinaryUploadService::Result result,
                                    DeepScanningClientResponse response);

// Access points used to record UMA metrics. Adding an access point here
// requires updating histograms.xml by adding histograms with names
//   "SafeBrowsing.DeepScan.<access-point>.BytesPerSeconds"
//   "SafeBrowsing.DeepScan.<access-point>.Duration"
//   "SafeBrowsing.DeepScan.<access-point>.<result>.Duration"
// TODO(domfc): Add PASTE access point.
enum class DeepScanAccessPoint {
  DOWNLOAD,
  UPLOAD,
  DRAG_AND_DROP,
};
std::string DeepScanAccessPointToString(DeepScanAccessPoint access_point);

// Helper functions to record DeepScanning UMA metrics for the duration of the
// request split by its result and bytes/sec for successful requests.
void RecordDeepScanMetrics(DeepScanAccessPoint access_point,
                           base::TimeDelta duration,
                           int64_t total_bytes,
                           const BinaryUploadService::Result& result,
                           const DeepScanningClientResponse& response);
void RecordDeepScanMetrics(DeepScanAccessPoint access_point,
                           base::TimeDelta duration,
                           int64_t total_bytes,
                           const std::string& result,
                           bool success);

}  // namespace safe_browsing

#endif  // CHROME_BROWSER_SAFE_BROWSING_CLOUD_CONTENT_SCANNING_DEEP_SCANNING_UTILS_H_
