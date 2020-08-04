// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/subresource_redirect/https_image_compression_bypass_decider.h"

#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/task_environment.h"
#include "base/time/time.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/common/features.h"

class HttpsImageCompressionBypassDeciderTest : public testing::Test {
 public:
  void SetUp() override {
    scoped_feature_list_.InitAndEnableFeature(
        blink::features::kSubresourceRedirect);
  }

 protected:
  base::test::SingleThreadTaskEnvironment task_environment_{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};
  base::test::ScopedFeatureList scoped_feature_list_;
  base::HistogramTester histogram_tester_;
  HttpsImageCompressionBypassDecider https_image_compression_bypass_decider_;
};

TEST_F(HttpsImageCompressionBypassDeciderTest, TestNoBypassOnInit) {
  EXPECT_FALSE(https_image_compression_bypass_decider_.ShouldBypassNow());
  histogram_tester_.ExpectUniqueSample(
      "SubresourceRedirect.PageLoad.BypassResult", false, 1);
}

// When an image fetch fails, it should be bypassed for a random duration.
TEST_F(HttpsImageCompressionBypassDeciderTest, TestRandomBypass) {
  https_image_compression_bypass_decider_.NotifyCompressedImageFetchFailed(
      base::TimeDelta());
  histogram_tester_.ExpectTotalCount("SubresourceRedirect.BypassDuration", 1);
  EXPECT_TRUE(https_image_compression_bypass_decider_.ShouldBypassNow());
  histogram_tester_.ExpectUniqueSample(
      "SubresourceRedirect.PageLoad.BypassResult", true, 1);

  // Image compression is bypassed until a minimum of one minute.
  task_environment_.FastForwardBy(base::TimeDelta::FromSeconds(59));
  EXPECT_TRUE(https_image_compression_bypass_decider_.ShouldBypassNow());
  histogram_tester_.ExpectBucketCount(
      "SubresourceRedirect.PageLoad.BypassResult", true, 2);

  // After another 5 minutes, bypass should get disabled.
  task_environment_.FastForwardBy(base::TimeDelta::FromMinutes(5));
  EXPECT_FALSE(https_image_compression_bypass_decider_.ShouldBypassNow());
  histogram_tester_.ExpectBucketCount(
      "SubresourceRedirect.PageLoad.BypassResult", false, 1);
}

TEST_F(HttpsImageCompressionBypassDeciderTest, TestExactBypass) {
  // Bypass for 30 seconds
  https_image_compression_bypass_decider_.NotifyCompressedImageFetchFailed(
      base::TimeDelta::FromSeconds(30));
  histogram_tester_.ExpectUniqueSample("SubresourceRedirect.BypassDuration",
                                       30000, 1);
  EXPECT_TRUE(https_image_compression_bypass_decider_.ShouldBypassNow());
  histogram_tester_.ExpectUniqueSample(
      "SubresourceRedirect.PageLoad.BypassResult", true, 1);

  task_environment_.FastForwardBy(base::TimeDelta::FromSeconds(31));
  EXPECT_FALSE(https_image_compression_bypass_decider_.ShouldBypassNow());
  histogram_tester_.ExpectBucketCount(
      "SubresourceRedirect.PageLoad.BypassResult", false, 1);
}

TEST_F(HttpsImageCompressionBypassDeciderTest, TestInvalidBypassDuration) {
  // Bypass for too long duration will limit the bypass to only 5 minutes.
  https_image_compression_bypass_decider_.NotifyCompressedImageFetchFailed(
      base::TimeDelta::FromMinutes(6));
  histogram_tester_.ExpectUniqueSample("SubresourceRedirect.BypassDuration",
                                       5 * 60 * 1000, 1);
  EXPECT_TRUE(https_image_compression_bypass_decider_.ShouldBypassNow());
  histogram_tester_.ExpectUniqueSample(
      "SubresourceRedirect.PageLoad.BypassResult", true, 1);

  task_environment_.FastForwardBy(base::TimeDelta::FromMinutes(6));
  EXPECT_FALSE(https_image_compression_bypass_decider_.ShouldBypassNow());
  histogram_tester_.ExpectBucketCount(
      "SubresourceRedirect.PageLoad.BypassResult", false, 1);
}
