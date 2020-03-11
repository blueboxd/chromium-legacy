// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/crash/core/common/crash_key.h"
#include "weblayer/browser/devtools_server_android.h"
#include "weblayer/browser/java/jni/WebLayerImpl_jni.h"

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

}  // namespace weblayer
