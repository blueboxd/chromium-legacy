// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "weblayer/browser/browser_callback_proxy.h"

#include "base/android/jni_string.h"
#include "url/gurl.h"
#include "weblayer/browser/browser_controller_impl.h"
#include "weblayer/browser/java/jni/BrowserCallbackProxy_jni.h"

using base::android::AttachCurrentThread;
using base::android::ConvertUTF8ToJavaString;
using base::android::ScopedJavaLocalRef;

namespace weblayer {

BrowserCallbackProxy::BrowserCallbackProxy(
    JNIEnv* env,
    jobject obj,
    BrowserController* browser_controller)
    : browser_controller_(browser_controller), java_observer_(env, obj) {
  browser_controller_->AddObserver(this);
}

BrowserCallbackProxy::~BrowserCallbackProxy() {
  browser_controller_->RemoveObserver(this);
}

void BrowserCallbackProxy::DisplayedUrlChanged(const GURL& url) {
  JNIEnv* env = AttachCurrentThread();
  ScopedJavaLocalRef<jstring> jstring_url(
      ConvertUTF8ToJavaString(env, url.spec()));
  Java_BrowserCallbackProxy_visibleUrlChanged(env, java_observer_, jstring_url);
}

static jlong JNI_BrowserCallbackProxy_CreateBrowserCallbackProxy(
    JNIEnv* env,
    const base::android::JavaParamRef<jobject>& proxy,
    jlong browser_controller) {
  return reinterpret_cast<jlong>(new BrowserCallbackProxy(
      env, proxy,
      reinterpret_cast<BrowserControllerImpl*>(browser_controller)));
}

static void JNI_BrowserCallbackProxy_DeleteBrowserCallbackProxy(JNIEnv* env,
                                                                jlong proxy) {
  delete reinterpret_cast<BrowserCallbackProxy*>(proxy);
}

}  // namespace weblayer
