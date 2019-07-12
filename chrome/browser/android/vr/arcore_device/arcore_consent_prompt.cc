// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/android/vr/arcore_device/arcore_consent_prompt.h"

#include <utility>

#include "base/bind.h"
#include "chrome/android/features/vr/jni_headers/ArConsentDialog_jni.h"
#include "chrome/browser/android/tab_android.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/web_contents.h"

using base::android::AttachCurrentThread;
using base::android::ScopedJavaLocalRef;

namespace vr {

namespace {

ArcoreConsentPrompt* g_instance = nullptr;

base::android::ScopedJavaLocalRef<jobject> GetTabFromRenderer(
    int render_process_id,
    int render_frame_id) {
  content::RenderFrameHost* render_frame_host =
      content::RenderFrameHost::FromID(render_process_id, render_frame_id);
  DCHECK(render_frame_host);

  content::WebContents* web_contents =
      content::WebContents::FromRenderFrameHost(render_frame_host);
  DCHECK(web_contents);

  TabAndroid* tab_android = TabAndroid::FromWebContents(web_contents);
  DCHECK(tab_android);

  base::android::ScopedJavaLocalRef<jobject> j_tab_android =
      tab_android->GetJavaObject();
  DCHECK(!j_tab_android.is_null());

  return j_tab_android;
}

}  // namespace

ArcoreConsentPrompt::ArcoreConsentPrompt() = default;

ArcoreConsentPrompt::~ArcoreConsentPrompt() = default;

void ArcoreConsentPrompt::GetUserPermission(
    int render_process_id,
    int render_frame_id,
    base::OnceCallback<void(bool)> response_callback) {
  on_user_consent_callback_ = std::move(response_callback);

  JNIEnv* env = AttachCurrentThread();
  ScopedJavaLocalRef<jobject> jdelegate = Java_ArConsentDialog_showDialog(
      env, reinterpret_cast<jlong>(this),
      GetTabFromRenderer(render_process_id, render_frame_id));
  if (jdelegate.is_null()) {
    std::move(on_user_consent_callback_).Run(false);
  }
}

void ArcoreConsentPrompt::OnUserConsentResult(
    JNIEnv* env,
    const base::android::JavaParamRef<jobject>& j_caller,
    jboolean is_granted) {
  DCHECK(on_user_consent_callback_);
  std::move(on_user_consent_callback_).Run(is_granted);
}

// static
void ArcoreConsentPrompt::ShowConsentPrompt(
    int render_process_id,
    int render_frame_id,
    base::OnceCallback<void(bool)> response_callback) {
  if (!g_instance)
    g_instance = new ArcoreConsentPrompt();
  g_instance->GetUserPermission(render_process_id, render_frame_id,
                                std::move(response_callback));
}

}  // namespace vr
