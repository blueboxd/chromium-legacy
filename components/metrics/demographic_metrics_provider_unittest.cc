// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/metrics/demographic_metrics_provider.h"

#include "base/test/scoped_feature_list.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/metrics_proto/chrome_user_metrics_extension.pb.h"

namespace metrics {
namespace {

TEST(DemographicMetricsProviderTest, ProvideCurrentSessionData_FeatureEnabled) {
  // Enable demographics reporting feature.
  base::test::ScopedFeatureList local_feature;
  local_feature.InitAndEnableFeature(
      DemographicMetricsProvider::kDemographicMetricsReporting);

  ChromeUserMetricsExtension uma_proto;

  DemographicMetricsProvider provider(/*pref_service=*/nullptr);
  provider.ProvideCurrentSessionData(&uma_proto);

  // Demographics are hard-coded in the provider for now, see crbug/979371.
  const int test_user_birthyear_bucket = 1990;
  const UserDemographicsProto::Gender test_user_gender =
      UserDemographicsProto::GENDER_UNKNOWN_OR_OTHER;

  EXPECT_EQ(test_user_birthyear_bucket,
            uma_proto.user_demographics().birth_year());
  EXPECT_EQ(test_user_gender, uma_proto.user_demographics().gender());
}

TEST(DemographicMetricsProviderTest,
     ProvideCurrentSessionData_FeatureDisabled) {
  // Disable demographics reporting feature.
  base::test::ScopedFeatureList local_feature;
  local_feature.InitAndDisableFeature(
      DemographicMetricsProvider::kDemographicMetricsReporting);

  ChromeUserMetricsExtension uma_proto;

  DemographicMetricsProvider provider(/*pref_service=*/nullptr);
  provider.ProvideCurrentSessionData(&uma_proto);

  // Expect that the UMA proto is untouched and has default data when feature
  // disabled.
  EXPECT_FALSE(uma_proto.user_demographics().has_birth_year());
  EXPECT_FALSE(uma_proto.user_demographics().has_gender());
}

}  // namespace
}  // namespace metrics