// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromecast/browser/android/cast_content_window_android.h"

#include <memory>

#include "base/android/jni_android.h"
#include "base/android/jni_string.h"
#include "base/android/scoped_java_ref.h"
#include "base/logging.h"
#include "base/memory/ptr_util.h"
#include "chromecast/browser/jni_headers/CastContentWindowAndroid_jni.h"
#include "content/public/browser/web_contents.h"
#include "ui/events/keycodes/keyboard_code_conversion_android.h"

namespace chromecast {

using base::android::ConvertUTF8ToJavaString;

namespace shell {

namespace {

base::android::ScopedJavaLocalRef<jobject> CreateJavaWindow(
    jlong native_window,
    bool is_headless,
    bool enable_touch_input,
    bool is_remote_control_mode,
    bool turn_on_screen,
    const std::string& session_id) {
  JNIEnv* env = base::android::AttachCurrentThread();
  return Java_CastContentWindowAndroid_create(
      env, native_window, is_headless, enable_touch_input,
      is_remote_control_mode, turn_on_screen,
      ConvertUTF8ToJavaString(env, session_id));
}

}  // namespace

// static
std::unique_ptr<CastContentWindow> CastContentWindow::Create(
    const CastContentWindow::CreateParams& params) {
  return base::WrapUnique(new CastContentWindowAndroid(params));
}

CastContentWindowAndroid::CastContentWindowAndroid(
    const CastContentWindow::CreateParams& params)
    : delegate_(params.delegate),
      java_window_(CreateJavaWindow(reinterpret_cast<jlong>(this),
                                    params.is_headless,
                                    params.enable_touch_input,
                                    params.is_remote_control_mode,
                                    params.turn_on_screen,
                                    params.session_id)) {
  DCHECK(delegate_);
}

CastContentWindowAndroid::~CastContentWindowAndroid() {
  JNIEnv* env = base::android::AttachCurrentThread();
  Java_CastContentWindowAndroid_onNativeDestroyed(env, java_window_);
}

void CastContentWindowAndroid::CreateWindowForWebContents(
    content::WebContents* web_contents,
    CastWindowManager* /* window_manager */,
    CastWindowManager::WindowId /* z_order */,
    VisibilityPriority visibility_priority) {
  DCHECK(web_contents);
  JNIEnv* env = base::android::AttachCurrentThread();
  base::android::ScopedJavaLocalRef<jobject> java_web_contents =
      web_contents->GetJavaWebContents();

  Java_CastContentWindowAndroid_createWindowForWebContents(
      env, java_window_, java_web_contents,
      static_cast<int>(visibility_priority));
}

void CastContentWindowAndroid::GrantScreenAccess() {
  NOTIMPLEMENTED();
}

void CastContentWindowAndroid::RevokeScreenAccess() {
  NOTIMPLEMENTED();
}

void CastContentWindowAndroid::EnableTouchInput(bool enabled) {
  JNIEnv* env = base::android::AttachCurrentThread();
  Java_CastContentWindowAndroid_enableTouchInput(
      env, java_window_, static_cast<jboolean>(enabled));
}

void CastContentWindowAndroid::OnActivityStopped(
    JNIEnv* env,
    const base::android::JavaParamRef<jobject>& jcaller) {
  delegate_->OnWindowDestroyed();
}

void CastContentWindowAndroid::OnKeyDown(
    JNIEnv* env,
    const base::android::JavaParamRef<jobject>& jcaller,
    int keycode) {
  ui::KeyEvent key_event(ui::ET_KEY_PRESSED,
                         ui::KeyboardCodeFromAndroidKeyCode(keycode),
                         ui::EF_NONE);
  delegate_->OnKeyEvent(key_event);
}

void CastContentWindowAndroid::RequestVisibility(
    VisibilityPriority visibility_priority) {
  JNIEnv* env = base::android::AttachCurrentThread();
  Java_CastContentWindowAndroid_requestVisibilityPriority(
      env, java_window_, static_cast<int>(visibility_priority));
}

void CastContentWindowAndroid::SetActivityContext(
    base::Value activity_context) {}

void CastContentWindowAndroid::SetHostContext(base::Value host_context) {}

void CastContentWindowAndroid::NotifyVisibilityChange(
    VisibilityType visibility_type) {
  delegate_->OnVisibilityChange(visibility_type);
  for (auto& observer : observer_list_) {
    observer.OnVisibilityChange(visibility_type);
  }
}

void CastContentWindowAndroid::RequestMoveOut() {
  JNIEnv* env = base::android::AttachCurrentThread();
  Java_CastContentWindowAndroid_requestMoveOut(env, java_window_);
}

bool CastContentWindowAndroid::ConsumeGesture(
    JNIEnv* env,
    const base::android::JavaParamRef<jobject>& jcaller,
    int gesture_type) {
  return delegate_->ConsumeGesture(static_cast<GestureType>(gesture_type));
}

void CastContentWindowAndroid::OnVisibilityChange(
    JNIEnv* env,
    const base::android::JavaParamRef<jobject>& jcaller,
    int visibility_type) {
  NotifyVisibilityChange(static_cast<VisibilityType>(visibility_type));
}

base::android::ScopedJavaLocalRef<jstring> CastContentWindowAndroid::GetId(
    JNIEnv* env,
    const base::android::JavaParamRef<jobject>& jcaller) {
  return ConvertUTF8ToJavaString(env, delegate_->GetId());
}
}  // namespace shell
}  // namespace chromecast
