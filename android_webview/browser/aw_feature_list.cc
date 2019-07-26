// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "android_webview/browser/aw_feature_list.h"

#include <string>

#include "android_webview/native_jni/AwFeatureList_jni.h"
#include "base/android/jni_string.h"
#include "base/feature_list.h"
#include "base/macros.h"
#include "base/stl_util.h"

using base::android::ConvertJavaStringToUTF8;
using base::android::JavaParamRef;

namespace android_webview {

namespace {

// Array of features exposed through the Java ChromeFeatureList API. Entries in
// this array may either refer to features defined in the header of this file or
// in other locations in the code base (e.g. content/, components/, etc).
const base::Feature* kFeaturesExposedToJava[] = {
    &features::kWebViewConnectionlessSafeBrowsing,
    &features::kWebViewPageStartedOnCommit,
};

const base::Feature* FindFeatureExposedToJava(const std::string& feature_name) {
  for (size_t i = 0; i < base::size(kFeaturesExposedToJava); ++i) {
    if (kFeaturesExposedToJava[i]->name == feature_name)
      return kFeaturesExposedToJava[i];
  }
  NOTREACHED() << "Queried feature cannot be found in AwFeatureList: "
               << feature_name;
  return nullptr;
}

}  // namespace

namespace features {

// Alphabetical:

// Enable brotli compression support in WebView.
const base::Feature kWebViewBrotliSupport{"WebViewBrotliSupport",
                                          base::FEATURE_DISABLED_BY_DEFAULT};

// Use the SafeBrowsingApiHandler which uses the connectionless GMS APIs. This
// Feature is checked and used in downstream internal code.
const base::Feature kWebViewConnectionlessSafeBrowsing{
    "WebViewConnectionlessSafeBrowsing", base::FEATURE_DISABLED_BY_DEFAULT};

// Kill switch for feature to call onPageFinished for browser-initiated
// navigations when the navigation commits.
const base::Feature kWebViewPageStartedOnCommit{
    "WebViewPageStartedOnCommit", base::FEATURE_ENABLED_BY_DEFAULT};

// Enable raster in wide color gamut for apps that use webview in a wide color
// gamut activity.
const base::Feature kWebViewWideColorGamutSupport{
    "WebViewWideColorGamutSupport", base::FEATURE_ENABLED_BY_DEFAULT};

}  // namespace features

static jboolean JNI_AwFeatureList_IsEnabled(
    JNIEnv* env,
    const JavaParamRef<jstring>& jfeature_name) {
  const base::Feature* feature =
      FindFeatureExposedToJava(ConvertJavaStringToUTF8(env, jfeature_name));
  return base::FeatureList::IsEnabled(*feature);
}

}  // namespace android_webview
