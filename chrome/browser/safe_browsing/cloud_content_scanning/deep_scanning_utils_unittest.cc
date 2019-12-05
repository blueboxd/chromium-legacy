// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/safe_browsing/cloud_content_scanning/deep_scanning_utils.h"

#include <string>
#include <tuple>

#include "base/test/metrics/histogram_tester.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace safe_browsing {

namespace {

constexpr BinaryUploadService::Result kAllBinaryUploadServiceResults[]{
    BinaryUploadService::Result::UNKNOWN,
    BinaryUploadService::Result::SUCCESS,
    BinaryUploadService::Result::UPLOAD_FAILURE,
    BinaryUploadService::Result::TIMEOUT,
    BinaryUploadService::Result::FILE_TOO_LARGE,
    BinaryUploadService::Result::FAILED_TO_GET_TOKEN,
    BinaryUploadService::Result::UNAUTHORIZED};

constexpr int64_t kTotalBytes = 1000;

constexpr base::TimeDelta kDuration = base::TimeDelta::FromSeconds(10);

constexpr base::TimeDelta kInvalidDuration = base::TimeDelta::FromSeconds(0);

std::string ResultToString(const BinaryUploadService::Result& result,
                           bool success) {
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
      return "";
  }
  return result_value;
}

}  // namespace

class DeepScanningUtilsUMATest
    : public testing::TestWithParam<
          std::tuple<DeepScanAccessPoint, BinaryUploadService::Result>> {
 public:
  DeepScanningUtilsUMATest() {}

  DeepScanAccessPoint access_point() const { return std::get<0>(GetParam()); }

  std::string access_point_string() const {
    return DeepScanAccessPointToString(access_point());
  }

  BinaryUploadService::Result result() const { return std::get<1>(GetParam()); }

  bool success() const {
    return result() == BinaryUploadService::Result::SUCCESS;
  }

  std::string result_value(bool success) const {
    return ResultToString(result(), success);
  }

  const base::HistogramTester& histograms() const { return histograms_; }

 private:
  base::HistogramTester histograms_;
};

INSTANTIATE_TEST_SUITE_P(
    Tests,
    DeepScanningUtilsUMATest,
    testing::Combine(testing::Values(DeepScanAccessPoint::DOWNLOAD,
                                     DeepScanAccessPoint::UPLOAD,
                                     DeepScanAccessPoint::DRAG_AND_DROP),
                     testing::ValuesIn(kAllBinaryUploadServiceResults)));

TEST_P(DeepScanningUtilsUMATest, SuccessfulScanVerdicts) {
  RecordDeepScanMetrics(access_point(), kDuration, kTotalBytes, result(),
                        DeepScanningClientResponse());

  if (result() == BinaryUploadService::Result::UNAUTHORIZED) {
    EXPECT_EQ(
        0u,
        histograms().GetTotalCountsForPrefix("SafeBrowsing.DeepScan.").size());
  } else {
    // We expect at least 2 histograms (<access-point>.Duration and
    // <access-point>.<result>.Duration), but only expect a third histogram in
    // the success case (bytes/seconds).
    uint64_t expected_histograms = success() ? 3u : 2u;
    EXPECT_EQ(
        expected_histograms,
        histograms().GetTotalCountsForPrefix("SafeBrowsing.DeepScan.").size());
    if (success()) {
      histograms().ExpectUniqueSample(
          "SafeBrowsing.DeepScan." + access_point_string() + ".BytesPerSeconds",
          kTotalBytes / kDuration.InSeconds(), 1);
    }
    histograms().ExpectTimeBucketCount(
        "SafeBrowsing.DeepScan." + access_point_string() + ".Duration",
        kDuration, 1);
    histograms().ExpectTimeBucketCount("SafeBrowsing.DeepScan." +
                                           access_point_string() + "." +
                                           result_value(true) + ".Duration",
                                       kDuration, 1);
  }
}

TEST_P(DeepScanningUtilsUMATest, UnsuccessfulDlpScanVerdict) {
  DlpDeepScanningVerdict dlp_verdict;
  dlp_verdict.set_status(DlpDeepScanningVerdict::FAILURE);
  DeepScanningClientResponse response;
  *response.mutable_dlp_scan_verdict() = dlp_verdict;

  RecordDeepScanMetrics(access_point(), kDuration, kTotalBytes, result(),
                        response);

  if (result() == BinaryUploadService::Result::UNAUTHORIZED) {
    EXPECT_EQ(
        0u,
        histograms().GetTotalCountsForPrefix("SafeBrowsing.DeepScan.").size());
  } else {
    EXPECT_EQ(
        2u,
        histograms().GetTotalCountsForPrefix("SafeBrowsing.DeepScan.").size());
    histograms().ExpectTimeBucketCount(
        "SafeBrowsing.DeepScan." + access_point_string() + ".Duration",
        kDuration, 1);
    histograms().ExpectTimeBucketCount("SafeBrowsing.DeepScan." +
                                           access_point_string() + "." +
                                           result_value(false) + ".Duration",
                                       kDuration, 1);
  }
}

TEST_P(DeepScanningUtilsUMATest, UnsuccessfulMalwareScanVerdict) {
  MalwareDeepScanningVerdict malware_verdict;
  malware_verdict.set_verdict(MalwareDeepScanningVerdict::VERDICT_UNSPECIFIED);
  DeepScanningClientResponse response;
  *response.mutable_malware_scan_verdict() = malware_verdict;

  RecordDeepScanMetrics(access_point(), kDuration, kTotalBytes, result(),
                        response);

  if (result() == BinaryUploadService::Result::UNAUTHORIZED) {
    EXPECT_EQ(
        0u,
        histograms().GetTotalCountsForPrefix("SafeBrowsing.DeepScan.").size());
  } else {
    EXPECT_EQ(
        2u,
        histograms().GetTotalCountsForPrefix("SafeBrowsing.DeepScan.").size());
    histograms().ExpectTimeBucketCount(
        "SafeBrowsing.DeepScan." + access_point_string() + ".Duration",
        kDuration, 1);
    histograms().ExpectTimeBucketCount("SafeBrowsing.DeepScan." +
                                           access_point_string() + "." +
                                           result_value(false) + ".Duration",
                                       kDuration, 1);
  }
}

TEST_P(DeepScanningUtilsUMATest, BypassScanVerdict) {
  RecordDeepScanMetrics(access_point(), kDuration, kTotalBytes,
                        "BypassedByUser", false);

  EXPECT_EQ(
      2u,
      histograms().GetTotalCountsForPrefix("SafeBrowsing.DeepScan.").size());
  histograms().ExpectTimeBucketCount(
      "SafeBrowsing.DeepScan." + access_point_string() + ".Duration", kDuration,
      1);
  histograms().ExpectTimeBucketCount("SafeBrowsing.DeepScan." +
                                         access_point_string() +
                                         ".BypassedByUser.Duration",
                                     kDuration, 1);
}

TEST_P(DeepScanningUtilsUMATest, CancelledByUser) {
  RecordDeepScanMetrics(access_point(), kDuration, kTotalBytes,
                        "CancelledByUser", false);

  EXPECT_EQ(
      2u,
      histograms().GetTotalCountsForPrefix("SafeBrowsing.DeepScan.").size());
  histograms().ExpectTimeBucketCount(
      "SafeBrowsing.DeepScan." + access_point_string() + ".Duration", kDuration,
      1);
  histograms().ExpectTimeBucketCount("SafeBrowsing.DeepScan." +
                                         access_point_string() +
                                         ".CancelledByUser.Duration",
                                     kDuration, 1);
}

TEST_P(DeepScanningUtilsUMATest, InvalidDuration) {
  RecordDeepScanMetrics(access_point(), kInvalidDuration, kTotalBytes, result(),
                        DeepScanningClientResponse());
  EXPECT_EQ(
      0u,
      histograms().GetTotalCountsForPrefix("SafeBrowsing.DeepScan.").size());
}

}  // namespace safe_browsing
