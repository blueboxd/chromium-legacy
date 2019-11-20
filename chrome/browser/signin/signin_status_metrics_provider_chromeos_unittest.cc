// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/signin/signin_status_metrics_provider_chromeos.h"

#include <string>

#include "base/test/metrics/histogram_tester.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {
const char kHistogramName[] = "UMA.ProfileSignInStatus";
}  // namespace

TEST(SigninStatusMetricsProviderChromeOS, ComputeSigninStatusToUpload) {
  SigninStatusMetricsProviderChromeOS metrics_provider;

  SigninStatusMetricsProviderBase::SigninStatus status_to_upload =
      metrics_provider.ComputeSigninStatusToUpload(
          SigninStatusMetricsProviderBase::ALL_PROFILES_SIGNED_IN, true);
  EXPECT_EQ(SigninStatusMetricsProviderBase::ALL_PROFILES_SIGNED_IN,
            status_to_upload);

  status_to_upload = metrics_provider.ComputeSigninStatusToUpload(
      SigninStatusMetricsProviderBase::ALL_PROFILES_NOT_SIGNED_IN, false);
  EXPECT_EQ(SigninStatusMetricsProviderBase::ALL_PROFILES_NOT_SIGNED_IN,
            status_to_upload);

  status_to_upload = metrics_provider.ComputeSigninStatusToUpload(
      SigninStatusMetricsProviderBase::ALL_PROFILES_NOT_SIGNED_IN, true);
  EXPECT_EQ(SigninStatusMetricsProviderBase::MIXED_SIGNIN_STATUS,
            status_to_upload);

  status_to_upload = metrics_provider.ComputeSigninStatusToUpload(
      SigninStatusMetricsProviderBase::ALL_PROFILES_SIGNED_IN, false);
  EXPECT_EQ(SigninStatusMetricsProviderBase::ERROR_GETTING_SIGNIN_STATUS,
            status_to_upload);
}

TEST(SigninStatusMetricsProviderChromeOS, ProvideCurrentSessionData_Guest) {
  SigninStatusMetricsProviderChromeOS::SetGuestForTesting(true);
  SigninStatusMetricsProviderChromeOS metrics_provider;

  auto check_histogram =
      [&metrics_provider](
          SigninStatusMetricsProviderBase::SigninStatus expected) {
        base::HistogramTester histogram_tester;
        metrics_provider.ProvideCurrentSessionData(nullptr);
        histogram_tester.ExpectUniqueSample(kHistogramName, expected, 1);
      };

  check_histogram(SigninStatusMetricsProviderBase::ALL_PROFILES_NOT_SIGNED_IN);
  check_histogram(SigninStatusMetricsProviderBase::ALL_PROFILES_NOT_SIGNED_IN);

  SigninStatusMetricsProviderChromeOS::SetGuestForTesting(false);
  check_histogram(SigninStatusMetricsProviderBase::MIXED_SIGNIN_STATUS);
  check_histogram(SigninStatusMetricsProviderBase::ALL_PROFILES_SIGNED_IN);
  check_histogram(SigninStatusMetricsProviderBase::ALL_PROFILES_SIGNED_IN);

  // We treat non-Guest to Guest transition as an error.
  SigninStatusMetricsProviderChromeOS::SetGuestForTesting(true);
  check_histogram(SigninStatusMetricsProviderBase::ERROR_GETTING_SIGNIN_STATUS);
}

TEST(SigninStatusMetricsProviderChromeOS, ProvideCurrentSessionData_NonGuest) {
  SigninStatusMetricsProviderChromeOS::SetGuestForTesting(false);
  SigninStatusMetricsProviderChromeOS metrics_provider;

  auto check_histogram =
      [&metrics_provider](
          SigninStatusMetricsProviderBase::SigninStatus expected) {
        base::HistogramTester histogram_tester;
        metrics_provider.ProvideCurrentSessionData(nullptr);
        histogram_tester.ExpectUniqueSample(kHistogramName, expected, 1);
      };

  check_histogram(SigninStatusMetricsProviderBase::ALL_PROFILES_SIGNED_IN);
  check_histogram(SigninStatusMetricsProviderBase::ALL_PROFILES_SIGNED_IN);

  // We treat non-Guest to Guest transition as an error.
  SigninStatusMetricsProviderChromeOS::SetGuestForTesting(true);
  check_histogram(SigninStatusMetricsProviderBase::ERROR_GETTING_SIGNIN_STATUS);
}
