// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/android/vr/gvr_consent_helper.h"

#include <utility>

#include "chrome/android/features/vr/jni_headers/VrConsentDialog_jni.h"
#include "chrome/browser/android/vr/android_vr_utils.h"

using base::android::AttachCurrentThread;

namespace vr {

GvrConsentHelper::GvrConsentHelper() : content::XrConsentHelper() {}

GvrConsentHelper::~GvrConsentHelper() {
  if (!jdelegate_.is_null()) {
    Java_VrConsentDialog_onNativeDestroy(AttachCurrentThread(), jdelegate_);
  }
}

void GvrConsentHelper::ShowConsentPrompt(
    int render_process_id,
    int render_frame_id,
    content::XrConsentPromptLevel consent_level,
    content::OnXrUserConsentCallback on_user_consent_callback) {
  DCHECK(!on_user_consent_callback_);
  on_user_consent_callback_ = std::move(on_user_consent_callback);
  consent_level_ = consent_level;

  JNIEnv* env = AttachCurrentThread();
  jdelegate_ = Java_VrConsentDialog_promptForUserConsent(
      env, reinterpret_cast<jlong>(this),
      GetTabFromRenderer(render_process_id, render_frame_id),
      static_cast<jint>(consent_level));
  if (jdelegate_.is_null()) {
    std::move(on_user_consent_callback_).Run(consent_level_, false);
    return;
  }
}

void GvrConsentHelper::OnUserConsentResult(JNIEnv* env, jboolean is_granted) {
  jdelegate_.Reset();
  if (on_user_consent_callback_)
    std::move(on_user_consent_callback_).Run(consent_level_, is_granted);
}

}  // namespace vr
