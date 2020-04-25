// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "weblayer/browser/android/metrics/weblayer_metrics_service_client.h"

#include <jni.h>
#include <cstdint>
#include <memory>

#include "base/no_destructor.h"
#include "components/metrics/metrics_service.h"
#include "components/version_info/android/channel_getter.h"
#include "weblayer/browser/java/jni/MetricsServiceClient_jni.h"

namespace weblayer {

namespace {

// IMPORTANT: DO NOT CHANGE sample rates without first ensuring the Chrome
// Metrics team has the appropriate backend bandwidth and storage.

// Sample at 10%, which is the same as chrome.
const int kStableSampledInRatePerMille = 100;

// Sample non-stable channels at 99%, to boost volume for pre-stable
// experiments. We choose 99% instead of 100% for consistency with Chrome and to
// exercise the out-of-sample code path.
const int kBetaDevCanarySampledInRatePerMille = 990;

// As a mitigation to preserve user privacy, the privacy team has asked that we
// upload package name with no more than 10% of UMA records. This is to mitigate
// fingerprinting for users on low-usage applications (if an app only has a
// a small handful of users, there's a very good chance many of them won't be
// uploading UMA records due to sampling). Do not change this constant without
// consulting with the privacy team.
const int kPackageNameLimitRatePerMille = 100;

}  // namespace

// static
WebLayerMetricsServiceClient* WebLayerMetricsServiceClient::GetInstance() {
  static base::NoDestructor<WebLayerMetricsServiceClient> client;
  client->EnsureOnValidSequence();
  return client.get();
}

WebLayerMetricsServiceClient::WebLayerMetricsServiceClient() = default;
WebLayerMetricsServiceClient::~WebLayerMetricsServiceClient() = default;

int32_t WebLayerMetricsServiceClient::GetProduct() {
  return metrics::ChromeUserMetricsExtension::ANDROID_WEBLAYER;
}

int WebLayerMetricsServiceClient::GetSampleRatePerMille() {
  version_info::Channel channel = version_info::android::GetChannel();
  if (channel == version_info::Channel::STABLE ||
      channel == version_info::Channel::UNKNOWN) {
    return kStableSampledInRatePerMille;
  }
  return kBetaDevCanarySampledInRatePerMille;
}

void WebLayerMetricsServiceClient::OnMetricsStart() {}

int WebLayerMetricsServiceClient::GetPackageNameLimitRatePerMille() {
  return kPackageNameLimitRatePerMille;
}

// static
void JNI_MetricsServiceClient_SetHaveMetricsConsent(JNIEnv* env,
                                                    jboolean user_consent,
                                                    jboolean app_consent) {
  WebLayerMetricsServiceClient::GetInstance()->SetHaveMetricsConsent(
      user_consent, app_consent);
}

}  // namespace weblayer
