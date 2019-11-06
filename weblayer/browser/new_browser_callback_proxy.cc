// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "weblayer/browser/new_browser_callback_proxy.h"

#include "base/android/jni_string.h"
#include "url/gurl.h"
#include "weblayer/browser/browser_controller_impl.h"
#include "weblayer/browser/java/jni/NewBrowserCallbackProxy_jni.h"

using base::android::AttachCurrentThread;
using base::android::ConvertUTF8ToJavaString;
using base::android::ScopedJavaLocalRef;

namespace weblayer {

NewBrowserCallbackProxy::NewBrowserCallbackProxy(
    JNIEnv* env,
    jobject obj,
    BrowserControllerImpl* browser_controller)
    : browser_controller_(browser_controller), java_impl_(env, obj) {
  DCHECK(!browser_controller_->has_new_browser_delegate());
  browser_controller_->SetNewBrowserDelegate(this);
}

NewBrowserCallbackProxy::~NewBrowserCallbackProxy() {
  browser_controller_->SetNewBrowserDelegate(nullptr);
}

void NewBrowserCallbackProxy::OnNewBrowser(
    std::unique_ptr<BrowserController> new_contents,
    NewBrowserType type) {
  JNIEnv* env = AttachCurrentThread();
  // The Java side takes ownership of BrowserController.
  Java_NewBrowserCallbackProxy_onNewBrowser(
      env, java_impl_, reinterpret_cast<jlong>(new_contents.release()),
      static_cast<int>(type));
}

static jlong JNI_NewBrowserCallbackProxy_CreateNewBrowserCallbackProxy(
    JNIEnv* env,
    const base::android::JavaParamRef<jobject>& proxy,
    jlong browser_controller) {
  return reinterpret_cast<jlong>(new NewBrowserCallbackProxy(
      env, proxy,
      reinterpret_cast<BrowserControllerImpl*>(browser_controller)));
}

static void JNI_NewBrowserCallbackProxy_DeleteNewBrowserCallbackProxy(
    JNIEnv* env,
    jlong proxy) {
  delete reinterpret_cast<NewBrowserCallbackProxy*>(proxy);
}

}  // namespace weblayer
