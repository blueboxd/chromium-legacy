// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "weblayer/browser/android/metrics/weblayer_metrics_service_client.h"

#include <jni.h>
#include <cstdint>
#include <memory>

#include "base/android/jni_android.h"
#include "base/android/jni_string.h"
#include "base/base_paths_android.h"
#include "base/no_destructor.h"
#include "components/metrics/android_metrics_provider.h"
#include "components/metrics/drive_metrics_provider.h"
#include "components/metrics/gpu/gpu_metrics_provider.h"
#include "components/metrics/metrics_service.h"
#include "components/metrics/version_utils.h"
#include "components/version_info/android/channel_getter.h"
#include "components/version_info/version_info.h"
#include "weblayer/browser/java/jni/MetricsServiceClient_jni.h"

namespace weblayer {

namespace {

// IMPORTANT: DO NOT CHANGE sample rates without first ensuring the Chrome
// Metrics team has the appropriate backend bandwidth and storage.

// Sample at 10%, which is the same as chrome.
const double kStableSampledInRate = 0.1;

// Sample non-stable channels at 99%, to boost volume for pre-stable
// experiments. We choose 99% instead of 100% for consistency with Chrome and to
// exercise the out-of-sample code path.
const double kBetaDevCanarySampledInRate = 0.99;

// As a mitigation to preserve user privacy, the privacy team has asked that we
// upload package name with no more than 10% of UMA records. This is to mitigate
// fingerprinting for users on low-usage applications (if an app only has a
// a small handful of users, there's a very good chance many of them won't be
// uploading UMA records due to sampling). Do not change this constant without
// consulting with the privacy team.
const double kPackageNameLimitRate = 0.10;

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

metrics::SystemProfileProto::Channel
WebLayerMetricsServiceClient::GetChannel() {
  return metrics::AsProtobufChannel(version_info::android::GetChannel());
}

std::string WebLayerMetricsServiceClient::GetVersionString() {
  return version_info::GetVersionNumber();
}

std::string WebLayerMetricsServiceClient::GetAppPackageNameInternal() {
  JNIEnv* env = base::android::AttachCurrentThread();
  base::android::ScopedJavaLocalRef<jstring> j_app_name =
      Java_MetricsServiceClient_getAppPackageName(env);
  if (j_app_name)
    return ConvertJavaStringToUTF8(env, j_app_name);
  return std::string();
}

double WebLayerMetricsServiceClient::GetSampleRate() {
  version_info::Channel channel = version_info::android::GetChannel();
  if (channel == version_info::Channel::STABLE ||
      channel == version_info::Channel::UNKNOWN) {
    return kStableSampledInRate;
  }
  return kBetaDevCanarySampledInRate;
}

void WebLayerMetricsServiceClient::InitInternal() {}

void WebLayerMetricsServiceClient::OnMetricsStart() {}

double WebLayerMetricsServiceClient::GetPackageNameLimitRate() {
  return kPackageNameLimitRate;
}

bool WebLayerMetricsServiceClient::ShouldWakeMetricsService() {
  return true;
}

void WebLayerMetricsServiceClient::RegisterAdditionalMetricsProviders(
    metrics::MetricsService* service) {
  service->RegisterMetricsProvider(
      std::make_unique<metrics::AndroidMetricsProvider>());
  service->RegisterMetricsProvider(
      std::make_unique<metrics::DriveMetricsProvider>(
          base::DIR_ANDROID_APP_DATA));
  service->RegisterMetricsProvider(
      std::make_unique<metrics::GPUMetricsProvider>());
}

bool WebLayerMetricsServiceClient::CanRecordPackageNameForAppType() {
  // Check with Java side, to see if it's OK to log the package name for this
  // type of app (see Java side for the specific requirements).
  JNIEnv* env = base::android::AttachCurrentThread();
  return Java_MetricsServiceClient_canRecordPackageNameForAppType(env);
}

// static
void JNI_MetricsServiceClient_SetHaveMetricsConsent(JNIEnv* env,
                                                    jboolean user_consent,
                                                    jboolean app_consent) {
  WebLayerMetricsServiceClient::GetInstance()->SetHaveMetricsConsent(
      user_consent, app_consent);
}

}  // namespace weblayer
