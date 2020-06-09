// Copyright (c) 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ANDROID_WEBVIEW_BROWSER_JS_JAVA_INTERACTION_JS_REPLY_PROXY_H_
#define ANDROID_WEBVIEW_BROWSER_JS_JAVA_INTERACTION_JS_REPLY_PROXY_H_

#include "base/android/jni_weak_ref.h"
#include "base/android/scoped_java_ref.h"

namespace android_webview {

class WebMessageReplyProxy;

class JsReplyProxy {
 public:
  explicit JsReplyProxy(WebMessageReplyProxy* reply_proxy);
  ~JsReplyProxy();

  base::android::ScopedJavaLocalRef<jobject> GetJavaPeer();

  void PostMessage(JNIEnv* env,
                   const base::android::JavaParamRef<jstring>& message);

 private:
  WebMessageReplyProxy* reply_proxy_;
  base::android::ScopedJavaGlobalRef<jobject> java_ref_;

  DISALLOW_COPY_AND_ASSIGN(JsReplyProxy);
};

}  // namespace android_webview

#endif  // ANDROID_WEBVIEW_BROWSER_JS_JAVA_INTERACTION_JS_REPLY_PROXY_H_
