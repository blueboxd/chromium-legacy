// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/external_intents/android/external_intents_feature_list.h"

#include <jni.h>
#include <stddef.h>
#include <string>

#include "base/android/jni_string.h"
#include "components/external_intents/android/jni_headers/ExternalIntentsFeatureList_jni.h"

namespace external_intents {

namespace {

// Array of features exposed through the Java ExternalIntentsFeatureList API.
const base::Feature* kFeaturesExposedToJava[] = {
    &kIntentBlockExternalFormRedirectsNoGesture,
};

const base::Feature* FindFeatureExposedToJava(const std::string& feature_name) {
  for (const auto* feature : kFeaturesExposedToJava) {
    if (feature->name == feature_name)
      return feature;
  }
  NOTREACHED()
      << "Queried feature cannot be found in ExternalIntentsFeatureList: "
      << feature_name;
  return nullptr;
}

}  // namespace

// Alphabetical:
const base::Feature kIntentBlockExternalFormRedirectsNoGesture{
    "IntentBlockExternalFormRedirectsNoGesture",
    base::FEATURE_DISABLED_BY_DEFAULT};

static jboolean JNI_ExternalIntentsFeatureList_IsInitialized(JNIEnv* env) {
  return !!base::FeatureList::GetInstance();
}

static jboolean JNI_ExternalIntentsFeatureList_IsEnabled(
    JNIEnv* env,
    const base::android::JavaParamRef<jstring>& jfeature_name) {
  const base::Feature* feature = FindFeatureExposedToJava(
      base::android::ConvertJavaStringToUTF8(env, jfeature_name));
  return base::FeatureList::IsEnabled(*feature);
}

}  // namespace external_intents
