// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/previews/core/previews_experiments.h"

#include <map>
#include <memory>
#include <string>
#include <vector>

#include "base/feature_list.h"
#include "base/metrics/field_trial.h"
#include "base/metrics/field_trial_params.h"
#include "base/strings/string_util.h"
#include "base/test/scoped_feature_list.h"
#include "build/build_config.h"
#include "components/previews/core/previews_constants.h"
#include "components/previews/core/previews_features.h"
#include "components/variations/variations_associated_data.h"

#include "testing/gtest/include/gtest/gtest.h"

namespace previews {

namespace {

const char kClientSidePreviewsFieldTrial[] = "ClientSidePreviews";
const char kEnabled[] = "Enabled";

// Verifies that the default params are correct, and that custom params can be
// set, for both the previews blacklist and offline previews.
TEST(PreviewsExperimentsTest, TestParamsForBlackListAndOffline) {
  // Verify that the default params are correct.
  EXPECT_EQ(4u, params::MaxStoredHistoryLengthForPerHostBlackList());
  EXPECT_EQ(10u, params::MaxStoredHistoryLengthForHostIndifferentBlackList());
  EXPECT_EQ(100u, params::MaxInMemoryHostsInBlackList());
  EXPECT_EQ(2, params::PerHostBlackListOptOutThreshold());
  EXPECT_EQ(6, params::HostIndifferentBlackListOptOutThreshold());
  EXPECT_EQ(base::TimeDelta::FromDays(30), params::PerHostBlackListDuration());
  EXPECT_EQ(base::TimeDelta::FromDays(30),
            params::HostIndifferentBlackListPerHostDuration());
  EXPECT_EQ(base::TimeDelta::FromSeconds(60 * 5),
            params::SingleOptOutDuration());
  EXPECT_EQ(base::TimeDelta::FromDays(7),
            params::OfflinePreviewFreshnessDuration());
  EXPECT_EQ(net::EFFECTIVE_CONNECTION_TYPE_2G,
            params::GetECTThresholdForPreview(PreviewsType::OFFLINE));
  EXPECT_EQ(0, params::OfflinePreviewsVersion());

  base::FieldTrialList field_trial_list(nullptr);

  // Set some custom params. Somewhat random yet valid values.
  std::map<std::string, std::string> custom_params = {
      {"per_host_max_stored_history_length", "3"},
      {"host_indifferent_max_stored_history_length", "4"},
      {"max_hosts_in_blacklist", "13"},
      {"per_host_opt_out_threshold", "12"},
      {"host_indifferent_opt_out_threshold", "84"},
      {"per_host_black_list_duration_in_days", "99"},
      {"host_indifferent_black_list_duration_in_days", "64"},
      {"single_opt_out_duration_in_seconds", "28"},
      {"offline_preview_freshness_duration_in_days", "12"},
      {"max_allowed_effective_connection_type", "4G"},
      {"version", "10"},
  };
  EXPECT_TRUE(base::AssociateFieldTrialParams(kClientSidePreviewsFieldTrial,
                                              kEnabled, custom_params));
  EXPECT_TRUE(base::FieldTrialList::CreateFieldTrial(
      kClientSidePreviewsFieldTrial, kEnabled));

  EXPECT_EQ(3u, params::MaxStoredHistoryLengthForPerHostBlackList());
  EXPECT_EQ(4u, params::MaxStoredHistoryLengthForHostIndifferentBlackList());
  EXPECT_EQ(13u, params::MaxInMemoryHostsInBlackList());
  EXPECT_EQ(12, params::PerHostBlackListOptOutThreshold());
  EXPECT_EQ(84, params::HostIndifferentBlackListOptOutThreshold());
  EXPECT_EQ(base::TimeDelta::FromDays(99), params::PerHostBlackListDuration());
  EXPECT_EQ(base::TimeDelta::FromDays(64),
            params::HostIndifferentBlackListPerHostDuration());
  EXPECT_EQ(base::TimeDelta::FromSeconds(28), params::SingleOptOutDuration());
  EXPECT_EQ(base::TimeDelta::FromDays(12),
            params::OfflinePreviewFreshnessDuration());
  EXPECT_EQ(net::EFFECTIVE_CONNECTION_TYPE_4G,
            params::GetECTThresholdForPreview(PreviewsType::OFFLINE));
  EXPECT_EQ(10, params::OfflinePreviewsVersion());

  variations::testing::ClearAllVariationParams();
}

TEST(PreviewsExperimentsTest, TestDefaultShouldExcludeMediaSuffix) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndEnableFeature(features::kExcludedMediaSuffixes);

  EXPECT_FALSE(
      params::ShouldExcludeMediaSuffix(GURL("http://chromium.org/path/")));

  std::vector<std::string> default_suffixes = {
      ".apk", ".avi",  ".gif", ".gifv", ".jpeg", ".jpg", ".mp3",
      ".mp4", ".mpeg", ".pdf", ".png",  ".webm", ".webp"};
  for (const std::string& suffix : default_suffixes) {
    GURL url("http://chromium.org/path/" + suffix);
    EXPECT_TRUE(params::ShouldExcludeMediaSuffix(url));
  }
}

TEST(PreviewsExperimentsTest, TestShouldExcludeMediaSuffix) {
  struct TestCase {
    std::string msg;
    bool enable_feature;
    std::string varaiation_value;
    std::vector<std::string> urls;
    bool want_return;
  };
  const TestCase kTestCases[]{
      {
          .msg = "Feature disabled, should always return false",
          .enable_feature = false,
          .varaiation_value = "",
          .urls = {"http://chromium.org/video.mp4"},
          .want_return = false,
      },
      {
          .msg = "Default values are overridden by variations",
          .enable_feature = true,
          .varaiation_value = ".html",
          .urls = {"http://chromium.org/video.mp4",
                   "http://chromium.org/image.png",
                   "http://chromium.org/image.jpg",
                   "http://chromium.org/audio.mp3"},
          .want_return = false,
      },
      {
          .msg = "Variation value whitespace should be trimmed",
          .enable_feature = true,
          .varaiation_value = " .mp4 , \t .png\n",
          .urls = {"http://chromium.org/video.mp4",
                   "http://chromium.org/image.png"},
          .want_return = true,
      },
      {
          .msg = "Variation value empty values should be excluded",
          .enable_feature = true,
          .varaiation_value = ".mp4,,.png,",
          .urls = {"http://chromium.org/video.mp4",
                   "http://chromium.org/image.png"},
          .want_return = true,
      },
      {
          .msg = "URLs should be compared case insensitive",
          .enable_feature = true,
          .varaiation_value = ".MP4,.png,",
          .urls = {"http://chromium.org/video.mP4",
                   "http://chromium.org/image.PNG"},
          .want_return = true,
      },
      {
          .msg = "Query params and fragments don't matter",
          .enable_feature = true,
          .varaiation_value = ".mp4,.png,",
          .urls = {"http://chromium.org/video.mp4?hello=world",
                   "http://chromium.org/image.png#test"},
          .want_return = true,
      },
      {
          .msg = "Query params and fragments shouldn't be considered",
          .enable_feature = true,
          .varaiation_value = ".mp4,.png,",
          .urls = {"http://chromium.org/?video=video.mp4",
                   "http://chromium.org/#image.png"},
          .want_return = false,
      },
  };
  for (const TestCase& test_case : kTestCases) {
    SCOPED_TRACE(test_case.msg);

    base::test::ScopedFeatureList scoped_feature_list;
    if (test_case.enable_feature) {
      scoped_feature_list.InitAndEnableFeatureWithParameters(
          features::kExcludedMediaSuffixes,
          {{"excluded_path_suffixes", test_case.varaiation_value}});
    } else {
      scoped_feature_list.InitAndDisableFeature(
          features::kExcludedMediaSuffixes);
    }

    for (const std::string& url : test_case.urls) {
      EXPECT_EQ(test_case.want_return,
                params::ShouldExcludeMediaSuffix(GURL(url)));
    }
  }
}

}  // namespace

}  // namespace previews
