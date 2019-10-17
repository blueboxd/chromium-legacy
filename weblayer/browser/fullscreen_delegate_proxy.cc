// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "weblayer/browser/fullscreen_delegate_proxy.h"

#include "base/android/jni_string.h"
#include "url/gurl.h"
#include "weblayer/browser/browser_controller_impl.h"
#include "weblayer/browser/java/jni/FullscreenDelegateProxy_jni.h"

using base::android::AttachCurrentThread;
using base::android::ConvertUTF8ToJavaString;
using base::android::ScopedJavaLocalRef;

namespace weblayer {

FullscreenDelegateProxy::FullscreenDelegateProxy(
    JNIEnv* env,
    jobject obj,
    BrowserController* browser_controller)
    : browser_controller_(browser_controller), java_observer_(env, obj) {
  browser_controller_->SetFullscreenDelegate(this);
}

FullscreenDelegateProxy::~FullscreenDelegateProxy() {
  browser_controller_->SetFullscreenDelegate(nullptr);
}

void FullscreenDelegateProxy::EnterFullscreen(base::OnceClosure exit_closure) {
  exit_fullscreen_closure_ = std::move(exit_closure);
  Java_FullscreenDelegateProxy_enterFullscreen(AttachCurrentThread(),
                                               java_observer_);
}

void FullscreenDelegateProxy::ExitFullscreen() {
  Java_FullscreenDelegateProxy_exitFullscreen(AttachCurrentThread(),
                                              java_observer_);
}

void FullscreenDelegateProxy::DoExitFullscreen(
    JNIEnv* env,
    const base::android::JavaParamRef<jobject>& caller) {
  if (exit_fullscreen_closure_)
    std::move(exit_fullscreen_closure_).Run();
}

static jlong JNI_FullscreenDelegateProxy_CreateFullscreenDelegateProxy(
    JNIEnv* env,
    const base::android::JavaParamRef<jobject>& proxy,
    jlong browser_controller) {
  return reinterpret_cast<jlong>(new FullscreenDelegateProxy(
      env, proxy,
      reinterpret_cast<BrowserControllerImpl*>(browser_controller)));
}

static void JNI_FullscreenDelegateProxy_DeleteFullscreenDelegateProxy(
    JNIEnv* env,
    jlong proxy) {
  delete reinterpret_cast<FullscreenDelegateProxy*>(proxy);
}

}  // namespace weblayer
