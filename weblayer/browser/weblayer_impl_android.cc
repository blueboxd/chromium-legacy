// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "weblayer/browser/weblayer_impl_android.h"
#include "base/android/jni_android.h"
#include "base/android/jni_string.h"
#include "components/crash/core/common/crash_key.h"
#include "components/page_info/android/page_info_client.h"
#include "weblayer/browser/devtools_server_android.h"
#include "weblayer/browser/java/jni/WebLayerImpl_jni.h"
#include "weblayer/browser/url_bar/page_info_client_impl.h"
#include "weblayer/browser/user_agent.h"

namespace weblayer {

static void JNI_WebLayerImpl_SetRemoteDebuggingEnabled(JNIEnv* env,
                                                       jboolean enabled) {
  DevToolsServerAndroid::SetRemoteDebuggingEnabled(enabled);
}

static jboolean JNI_WebLayerImpl_IsRemoteDebuggingEnabled(JNIEnv* env) {
  return DevToolsServerAndroid::GetRemoteDebuggingEnabled();
}

static void JNI_WebLayerImpl_SetIsWebViewCompatMode(JNIEnv* env,
                                                    jboolean value) {
  static crash_reporter::CrashKeyString<1> crash_key(
      "WEBLAYER_WEB_VIEW_COMPAT_MODE");
  crash_key.Set(value ? "1" : "0");
}

static base::android::ScopedJavaLocalRef<jstring>
JNI_WebLayerImpl_GetUserAgentString(JNIEnv* env) {
  return base::android::ConvertUTF8ToJavaString(
      base::android::AttachCurrentThread(), GetUserAgent());
}

base::string16 GetClientApplicationName() {
  JNIEnv* env = base::android::AttachCurrentThread();

  return base::android::ConvertJavaStringToUTF16(
      env, Java_WebLayerImpl_getEmbedderName(env));
}

}  // namespace weblayer
