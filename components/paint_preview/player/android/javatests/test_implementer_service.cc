// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/paint_preview/player/android/javatests/test_implementer_service.h"

#include <memory>

#include "base/android/jni_android.h"
#include "base/android/jni_string.h"
#include "base/android/scoped_java_ref.h"
#include "base/files/file_path.h"
#include "components/paint_preview/browser/paint_preview_base_service.h"
#include "components/paint_preview/browser/test_paint_preview_policy.h"
#include "components/paint_preview/player/android/javatests_jni_headers/TestImplementerService_jni.h"

using base::android::JavaParamRef;

namespace paint_preview {

jlong JNI_TestImplementerService_GetInstance(
    JNIEnv* env,
    const JavaParamRef<jstring>& j_string_path) {
  base::FilePath file_path(
      base::android::ConvertJavaStringToUTF8(env, j_string_path));
  TestImplementerService* service =
      new TestImplementerService(file_path, false);
  return reinterpret_cast<intptr_t>(service);
}

TestImplementerService::TestImplementerService(
    const base::FilePath& profile_path,
    bool is_off_the_record)
    : PaintPreviewBaseService(profile_path,
                              "TestImplementerService",
                              std::make_unique<TestPaintPreviewPolicy>(),
                              is_off_the_record) {}

TestImplementerService::~TestImplementerService() = default;

}  // namespace paint_preview
