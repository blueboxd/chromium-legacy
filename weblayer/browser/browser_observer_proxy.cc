// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "weblayer/browser/browser_observer_proxy.h"

#include "base/android/jni_string.h"
#include "url/gurl.h"
#include "weblayer/browser/browser_controller_impl.h"
#include "weblayer/browser/java/jni/BrowserObserverProxy_jni.h"

using base::android::AttachCurrentThread;
using base::android::ConvertUTF8ToJavaString;
using base::android::ScopedJavaLocalRef;

namespace weblayer {

BrowserObserverProxy::BrowserObserverProxy(
    JNIEnv* env,
    jobject obj,
    BrowserController* browser_controller)
    : browser_controller_(browser_controller), java_observer_(env, obj) {
  browser_controller_->AddObserver(this);
}

BrowserObserverProxy::~BrowserObserverProxy() {
  browser_controller_->RemoveObserver(this);
}

void BrowserObserverProxy::DisplayedURLChanged(const GURL& url) {
  JNIEnv* env = AttachCurrentThread();
  ScopedJavaLocalRef<jstring> jstring_url(
      ConvertUTF8ToJavaString(env, url.spec()));
  Java_BrowserObserverProxy_displayURLChanged(env, java_observer_, jstring_url);
}

void BrowserObserverProxy::LoadingStateChanged(bool is_loading,
                                               bool to_different_document) {}

void BrowserObserverProxy::LoadProgressChanged(double progress) {}

void BrowserObserverProxy::FirstContentfulPaint() {}

static jlong JNI_BrowserObserverProxy_CreateBrowserObserverProxy(
    JNIEnv* env,
    const base::android::JavaParamRef<jobject>& proxy,
    jlong browser_controller) {
  return reinterpret_cast<jlong>(new BrowserObserverProxy(
      env, proxy,
      reinterpret_cast<BrowserControllerImpl*>(browser_controller)));
}

static void JNI_BrowserObserverProxy_DeleteBrowserObserverProxy(JNIEnv* env,
                                                                jlong proxy) {
  delete reinterpret_cast<BrowserObserverProxy*>(proxy);
}

}  // namespace weblayer
