// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <jni.h>

#include "base/android/jni_string.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/share/android/jni_headers/BitmapUriRequest_jni.h"
#include "chrome/browser/share/qr_code_generation_request.h"
#include "content/public/browser/download_manager.h"
#include "content/public/browser/download_request_utils.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "ui/base/webui/web_ui_util.h"
#include "ui/gfx/android/java_bitmap.h"

using base::android::JavaParamRef;

static base::android::ScopedJavaLocalRef<jstring>
JNI_BitmapUriRequest_BitmapUri(JNIEnv* env,
                               const JavaParamRef<jobject>& j_bitmap) {
  SkBitmap bitmap =
      gfx::CreateSkBitmapFromJavaBitmap(gfx::JavaBitmap(j_bitmap));

  return base::android::ConvertUTF8ToJavaString(
      env, webui::GetBitmapDataUrl(bitmap));
}
