// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/android/feature_utilities.h"

#include "chrome/android/chrome_jni_headers/FeatureUtilities_jni.h"

#include "base/android/jni_string.h"
#include "content/public/common/content_features.h"
#include "content/public/common/network_service_util.h"

using base::android::ConvertJavaStringToUTF8;
using base::android::JavaParamRef;
using base::android::ScopedJavaLocalRef;

namespace chrome {
namespace android {

bool IsDownloadAutoResumptionEnabledInNative() {
  JNIEnv* env = base::android::AttachCurrentThread();
  return Java_FeatureUtilities_isDownloadAutoResumptionEnabledInNative(env);
}

std::string GetReachedCodeProfilerTrialGroup() {
  JNIEnv* env = base::android::AttachCurrentThread();
  ScopedJavaLocalRef<jstring> group =
      Java_FeatureUtilities_getReachedCodeProfilerTrialGroup(env);
  return ConvertJavaStringToUTF8(env, group);
}

} // namespace android
} // namespace chrome

static jboolean JNI_FeatureUtilities_IsNetworkServiceWarmUpEnabled(
    JNIEnv* env) {
  return content::IsOutOfProcessNetworkService() &&
         base::FeatureList::IsEnabled(features::kWarmUpNetworkProcess);
}
