// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/android/vr/arcore_device/arcore_consent_prompt.h"

#include <memory>
#include <utility>

#include "base/bind.h"
#include "chrome/android/features/vr/jni_headers/ArConsentDialog_jni.h"
#include "chrome/browser/android/vr/android_vr_utils.h"

using base::android::AttachCurrentThread;

namespace vr {

ArCoreConsentPrompt::ArCoreConsentPrompt()
    : content::XrConsentHelper(), weak_ptr_factory_(this) {}

ArCoreConsentPrompt::~ArCoreConsentPrompt() {
  if (!jdelegate_.is_null()) {
    Java_ArConsentDialog_onNativeDestroy(AttachCurrentThread(), jdelegate_);
  }
}

void ArCoreConsentPrompt::ShowConsentPrompt(
    int render_process_id,
    int render_frame_id,
    content::XrConsentPromptLevel consent_level,
    content::OnXrUserConsentCallback response_callback) {
  DCHECK(!on_user_consent_callback_);
  on_user_consent_callback_ = std::move(response_callback);
  consent_level_ = consent_level;

  JNIEnv* env = AttachCurrentThread();
  jdelegate_ = Java_ArConsentDialog_showDialog(
      env, reinterpret_cast<jlong>(this),
      GetTabFromRenderer(render_process_id, render_frame_id));
  if (jdelegate_.is_null()) {
    CallDeferredUserConsentCallback(false);
  }
}

void ArCoreConsentPrompt::OnUserConsentResult(JNIEnv* env,
                                              jboolean is_granted) {
  jdelegate_.Reset();
  CallDeferredUserConsentCallback(is_granted);
}

void ArCoreConsentPrompt::CallDeferredUserConsentCallback(
    bool is_permission_granted) {
  if (on_user_consent_callback_) {
    std::move(on_user_consent_callback_)
        .Run(consent_level_, is_permission_granted);
  }
}

}  // namespace vr
