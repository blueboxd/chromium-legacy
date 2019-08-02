// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/android/overlay/overlay_window_android.h"

#include "base/android/jni_android.h"
#include "base/memory/ptr_util.h"
#include "chrome/android/chrome_jni_headers/PictureInPictureActivity_jni.h"
#include "chrome/browser/android/tab_android.h"
#include "content/public/browser/overlay_window.h"
#include "content/public/browser/picture_in_picture_window_controller.h"
#include "ui/views/widget/widget.h"

// static
std::unique_ptr<content::OverlayWindow> content::OverlayWindow::Create(
    PictureInPictureWindowController* controller) {
  return std::make_unique<OverlayWindowAndroid>(controller);
}

OverlayWindowAndroid::OverlayWindowAndroid(
    content::PictureInPictureWindowController* controller)
    : controller_(controller) {
  JNIEnv* env = base::android::AttachCurrentThread();
  Java_PictureInPictureActivity_createActivity(
      env, reinterpret_cast<long>(this),
      TabAndroid::FromWebContents(controller_->GetInitiatorWebContents())
          ->GetJavaObject());
}

OverlayWindowAndroid::~OverlayWindowAndroid() {
  JNIEnv* env = base::android::AttachCurrentThread();
  Java_PictureInPictureActivity_onWindowDestroyed(env,
                                                  reinterpret_cast<long>(this));
}

void OverlayWindowAndroid::OnActivityStart(
    JNIEnv* env,
    const base::android::JavaParamRef<jobject>& obj,
    const base::android::JavaParamRef<jobject>& jwindow_android) {
  java_ref_ = JavaObjectWeakGlobalRef(env, obj);
  window_android_ = ui::WindowAndroid::FromJavaWindowAndroid(jwindow_android);
  window_android_->AddObserver(this);
}

void OverlayWindowAndroid::OnActivityStopped() {
  Destroy(nullptr);
}

void OverlayWindowAndroid::Destroy(JNIEnv* env) {
  java_ref_.reset();

  if (window_android_) {
    window_android_->RemoveObserver(this);
    window_android_ = nullptr;
  }

  controller_->CloseAndFocusInitiator();
  controller_->OnWindowDestroyed();
}

void OverlayWindowAndroid::Close() {
  if (java_ref_.is_uninitialized())
    return;

  DCHECK(window_android_);
  window_android_->RemoveObserver(this);
  window_android_ = nullptr;
  JNIEnv* env = base::android::AttachCurrentThread();
  Java_PictureInPictureActivity_close(env, java_ref_.get(env));
  controller_->OnWindowDestroyed();
}

void OverlayWindowAndroid::Hide() {
  Close();
}

bool OverlayWindowAndroid::IsActive() {
  return true;
}

bool OverlayWindowAndroid::IsVisible() {
  return true;
}

bool OverlayWindowAndroid::IsAlwaysOnTop() {
  return true;
}

gfx::Rect OverlayWindowAndroid::GetBounds() {
  return gfx::Rect();
}

cc::Layer* OverlayWindowAndroid::GetLayerForTesting() {
  return nullptr;
}
