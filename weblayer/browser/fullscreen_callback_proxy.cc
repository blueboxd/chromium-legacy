// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "weblayer/browser/fullscreen_callback_proxy.h"

#include "base/android/jni_string.h"
#include "url/gurl.h"
#include "weblayer/browser/browser_controller_impl.h"
#include "weblayer/browser/java/jni/FullscreenCallbackProxy_jni.h"

using base::android::AttachCurrentThread;
using base::android::ConvertUTF8ToJavaString;
using base::android::ScopedJavaLocalRef;

namespace weblayer {

FullscreenCallbackProxy::FullscreenCallbackProxy(
    JNIEnv* env,
    jobject obj,
    BrowserController* browser_controller)
    : browser_controller_(browser_controller), java_delegate_(env, obj) {
  browser_controller_->SetFullscreenDelegate(this);
}

FullscreenCallbackProxy::~FullscreenCallbackProxy() {
  browser_controller_->SetFullscreenDelegate(nullptr);
}

void FullscreenCallbackProxy::EnterFullscreen(base::OnceClosure exit_closure) {
  exit_fullscreen_closure_ = std::move(exit_closure);
  Java_FullscreenCallbackProxy_enterFullscreen(AttachCurrentThread(),
                                               java_delegate_);
}

void FullscreenCallbackProxy::ExitFullscreen() {
  Java_FullscreenCallbackProxy_exitFullscreen(AttachCurrentThread(),
                                              java_delegate_);
}

void FullscreenCallbackProxy::DoExitFullscreen(
    JNIEnv* env,
    const base::android::JavaParamRef<jobject>& caller) {
  if (exit_fullscreen_closure_)
    std::move(exit_fullscreen_closure_).Run();
}

static jlong JNI_FullscreenCallbackProxy_CreateFullscreenCallbackProxy(
    JNIEnv* env,
    const base::android::JavaParamRef<jobject>& proxy,
    jlong browser_controller) {
  return reinterpret_cast<jlong>(new FullscreenCallbackProxy(
      env, proxy,
      reinterpret_cast<BrowserControllerImpl*>(browser_controller)));
}

static void JNI_FullscreenCallbackProxy_DeleteFullscreenCallbackProxy(
    JNIEnv* env,
    jlong proxy) {
  delete reinterpret_cast<FullscreenCallbackProxy*>(proxy);
}

}  // namespace weblayer
