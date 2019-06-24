// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/common/safe_browsing/file_type_policies.h"

#include "base/android/jni_string.h"
#include "base/files/file_path.h"
#include "chrome/android/chrome_jni_headers/FileTypePolicies_jni.h"

namespace safe_browsing {

static jint JNI_FileTypePolicies_UmaValueForFile(
    JNIEnv* env,
    const base::android::JavaParamRef<jstring>& path) {
  base::FilePath file_path(ConvertJavaStringToUTF8(env, path));
  return safe_browsing::FileTypePolicies::GetInstance()->UmaValueForFile(
      file_path);
}

}  // namespace safe_browsing
