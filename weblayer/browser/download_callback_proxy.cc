// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "weblayer/browser/download_callback_proxy.h"

#include "base/android/jni_string.h"
#include "url/gurl.h"
#include "weblayer/browser/browser_controller_impl.h"
#include "weblayer/browser/java/jni/DownloadCallbackProxy_jni.h"

using base::android::AttachCurrentThread;
using base::android::ConvertUTF8ToJavaString;
using base::android::ScopedJavaLocalRef;

namespace weblayer {

DownloadCallbackProxy::DownloadCallbackProxy(
    JNIEnv* env,
    jobject obj,
    BrowserController* browser_controller)
    : browser_controller_(browser_controller), java_delegate_(env, obj) {
  browser_controller_->SetDownloadDelegate(this);
}

DownloadCallbackProxy::~DownloadCallbackProxy() {
  browser_controller_->SetDownloadDelegate(nullptr);
}

void DownloadCallbackProxy::DownloadRequested(
    const GURL& url,
    const std::string& user_agent,
    const std::string& content_disposition,
    const std::string& mime_type,
    int64_t content_length) {
  JNIEnv* env = AttachCurrentThread();
  ScopedJavaLocalRef<jstring> jstring_url(
      ConvertUTF8ToJavaString(env, url.spec()));
  ScopedJavaLocalRef<jstring> jstring_user_agent(
      ConvertUTF8ToJavaString(env, user_agent));
  ScopedJavaLocalRef<jstring> jstring_content_disposition(
      ConvertUTF8ToJavaString(env, content_disposition));
  ScopedJavaLocalRef<jstring> jstring_mime_type(
      ConvertUTF8ToJavaString(env, mime_type));
  Java_DownloadCallbackProxy_downloadRequested(
      env, java_delegate_, jstring_url, jstring_user_agent,
      jstring_content_disposition, jstring_mime_type, content_length);
}

static jlong JNI_DownloadCallbackProxy_CreateDownloadCallbackProxy(
    JNIEnv* env,
    const base::android::JavaParamRef<jobject>& proxy,
    jlong browser_controller) {
  return reinterpret_cast<jlong>(new DownloadCallbackProxy(
      env, proxy,
      reinterpret_cast<BrowserControllerImpl*>(browser_controller)));
}

static void JNI_DownloadCallbackProxy_DeleteDownloadCallbackProxy(JNIEnv* env,
                                                                  jlong proxy) {
  delete reinterpret_cast<DownloadCallbackProxy*>(proxy);
}

}  // namespace weblayer
