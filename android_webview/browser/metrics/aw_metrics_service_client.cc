// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "android_webview/browser/metrics/aw_metrics_service_client.h"

#include <jni.h>
#include <cstdint>

#include "android_webview/browser/lifecycle/aw_contents_lifecycle_notifier.h"
#include "android_webview/browser/metrics/aw_stability_metrics_provider.h"
#include "android_webview/browser_jni_headers/AwMetricsServiceClient_jni.h"
#include "android_webview/common/aw_features.h"
#include "base/android/jni_android.h"
#include "base/feature_list.h"
#include "base/metrics/histogram_functions.h"
#include "base/no_destructor.h"
#include "components/metrics/metrics_pref_names.h"
#include "components/metrics/metrics_service.h"
#include "components/prefs/pref_service.h"
#include "components/version_info/android/channel_getter.h"

namespace android_webview {

namespace {

// IMPORTANT: DO NOT CHANGE sample rates without first ensuring the Chrome
// Metrics team has the appropriate backend bandwidth and storage.

// Sample at 2%, based on storage concerns. We sample at a different rate than
// Chrome because we have more metrics "clients" (each app on the device counts
// as a separate client).
const int kStableSampledInRatePerMille = 20;

// Sample non-stable channels at 99%, to boost volume for pre-stable
// experiments. We choose 99% instead of 100% for consistency with Chrome and to
// exercise the out-of-sample code path.
const int kBetaDevCanarySampledInRatePerMille = 990;

// As a mitigation to preserve use privacy, the privacy team has asked that we
// upload package name with no more than 10% of UMA clients. This is to mitigate
// fingerprinting for users on low-usage applications (if an app only has a
// a small handful of users, there's a very good chance many of them won't be
// uploading UMA records due to sampling). Do not change this constant without
// consulting with the privacy team.
const int kPackageNameLimitRatePerMille = 100;

}  // namespace

// static
AwMetricsServiceClient* AwMetricsServiceClient::GetInstance() {
  static base::NoDestructor<AwMetricsServiceClient> client;
  client->EnsureOnValidSequence();
  return client.get();
}

AwMetricsServiceClient::AwMetricsServiceClient() = default;
AwMetricsServiceClient::~AwMetricsServiceClient() = default;

int32_t AwMetricsServiceClient::GetProduct() {
  return metrics::ChromeUserMetricsExtension::ANDROID_WEBVIEW;
}

int AwMetricsServiceClient::GetSampleRatePerMille() {
  // Down-sample unknown channel as a precaution in case it ends up being
  // shipped to Stable users.
  version_info::Channel channel = version_info::android::GetChannel();
  if (channel == version_info::Channel::STABLE ||
      channel == version_info::Channel::UNKNOWN) {
    return kStableSampledInRatePerMille;
  }
  return kBetaDevCanarySampledInRatePerMille;
}

void AwMetricsServiceClient::OnMetricsStart() {
  AwContentsLifecycleNotifier::GetInstance().AddObserver(this);
}

int AwMetricsServiceClient::GetPackageNameLimitRatePerMille() {
  return kPackageNameLimitRatePerMille;
}

bool AwMetricsServiceClient::ShouldWakeMetricsService() {
  return base::FeatureList::IsEnabled(features::kWebViewWakeMetricsService);
}

void AwMetricsServiceClient::OnAppStateChanged(
    WebViewAppStateObserver::State state) {
  // To match MetricsService's expectation,
  // - does nothing if no WebView has ever been created.
  // - starts notifying MetricsService once a WebView is created and the app
  //   is foreground.
  // - consolidates the other states other than kForeground into background.
  // - avoids the duplicated notification.
  if (state == WebViewAppStateObserver::State::kDestroyed &&
      !AwContentsLifecycleNotifier::GetInstance()
           .has_aw_contents_ever_created()) {
    return;
  }

  bool foreground = state == WebViewAppStateObserver::State::kForeground;

  if (foreground == app_in_foreground_)
    return;

  app_in_foreground_ = foreground;
  if (app_in_foreground_) {
    GetMetricsService()->OnAppEnterForeground();
  } else {
    // TODO(https://crbug.com/1052392): Turn on the background recording.
    // Not recording in background, this matches Chrome's behavior.
    GetMetricsService()->OnAppEnterBackground(
        /* keep_recording_in_background = false */);
  }
}

void AwMetricsServiceClient::RegisterAdditionalMetricsProviders(
    metrics::MetricsService* service) {
  if (base::FeatureList::IsEnabled(features::kWebViewWakeMetricsService)) {
    service->RegisterMetricsProvider(
        std::make_unique<android_webview::AwStabilityMetricsProvider>(
            pref_service()));
  }
}

// static
void JNI_AwMetricsServiceClient_SetHaveMetricsConsent(JNIEnv* env,
                                                      jboolean user_consent,
                                                      jboolean app_consent) {
  AwMetricsServiceClient::GetInstance()->SetHaveMetricsConsent(user_consent,
                                                               app_consent);
}

// static
void JNI_AwMetricsServiceClient_SetFastStartupForTesting(
    JNIEnv* env,
    jboolean fast_startup_for_testing) {
  AwMetricsServiceClient::GetInstance()->SetFastStartupForTesting(
      fast_startup_for_testing);
}

// static
void JNI_AwMetricsServiceClient_SetUploadIntervalForTesting(
    JNIEnv* env,
    jlong upload_interval_ms) {
  AwMetricsServiceClient::GetInstance()->SetUploadIntervalForTesting(
      base::TimeDelta::FromMilliseconds(upload_interval_ms));
}

}  // namespace android_webview
