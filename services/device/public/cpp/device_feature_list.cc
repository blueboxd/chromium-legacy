// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/android/jni_string.h"
#include "base/feature_list.h"
#include "base/stl_util.h"
#include "services/device/device_service_jni_headers/DeviceFeatureList_jni.h"
#include "services/device/public/cpp/device_features.h"

using base::android::ConvertJavaStringToUTF8;
using base::android::JavaParamRef;

namespace features {

namespace {

// Array of features exposed through the Java ContentFeatureList API. Entries in
// this array may either refer to features defined in the header of this file or
// in other locations in the code base (e.g. content_features.h).
const base::Feature* kFeaturesExposedToJava[] = {
    &kGenericSensorExtraClasses,
};

// TODO(crbug.com/1060097): Removethis once a generalized FeatureList exists.
const base::Feature* FindFeatureExposedToJava(const std::string& feature_name) {
  for (size_t i = 0; i < base::size(kFeaturesExposedToJava); ++i) {
    if (kFeaturesExposedToJava[i]->name == feature_name)
      return kFeaturesExposedToJava[i];
  }
  NOTREACHED() << "Queried feature cannot be found in DeviceFeatureList: "
               << feature_name;
  return nullptr;
}

}  // namespace

static jboolean JNI_DeviceFeatureList_IsInitialized(JNIEnv* env) {
  return !!base::FeatureList::GetInstance();
}

static jboolean JNI_DeviceFeatureList_IsEnabled(
    JNIEnv* env,
    const JavaParamRef<jstring>& jfeature_name) {
  const base::Feature* feature =
      FindFeatureExposedToJava(ConvertJavaStringToUTF8(env, jfeature_name));
  return base::FeatureList::IsEnabled(*feature);
}

}  // namespace features
