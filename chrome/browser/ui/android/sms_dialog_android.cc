// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/android/sms_dialog_android.h"

#include "base/android/jni_android.h"
#include "base/android/jni_string.h"
#include "chrome/android/chrome_jni_headers/SmsReceiverDialog_jni.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/web_contents.h"
#include "ui/android/window_android.h"

using base::android::AttachCurrentThread;
using base::android::JavaParamRef;
using base::android::ScopedJavaLocalRef;

SmsDialogAndroid::SmsDialogAndroid() {
  java_dialog_.Reset(Java_SmsReceiverDialog_create(
      AttachCurrentThread(), reinterpret_cast<intptr_t>(this)));
}

SmsDialogAndroid::~SmsDialogAndroid() {
  Java_SmsReceiverDialog_destroy(AttachCurrentThread(), java_dialog_);
}

void SmsDialogAndroid::Open(content::RenderFrameHost* host,
                            base::OnceClosure on_continue,
                            base::OnceClosure on_cancel) {
  on_continue_ = std::move(on_continue);
  on_cancel_ = std::move(on_cancel);

  content::WebContents* web_contents =
      content::WebContents::FromRenderFrameHost(host);

  base::android::ScopedJavaLocalRef<jobject> window_android =
      web_contents->GetNativeView()->GetWindowAndroid()->GetJavaObject();

  Java_SmsReceiverDialog_open(AttachCurrentThread(), java_dialog_,
                              window_android);
}

void SmsDialogAndroid::Close() {
  Java_SmsReceiverDialog_close(AttachCurrentThread(), java_dialog_);
}

void SmsDialogAndroid::EnableContinueButton() {
  Java_SmsReceiverDialog_enableContinueButton(AttachCurrentThread(),
                                              java_dialog_);
}

void SmsDialogAndroid::OnContinue(
    JNIEnv* env,
    const base::android::JavaParamRef<jobject>& obj) {
  std::move(on_continue_).Run();
}

void SmsDialogAndroid::OnCancel(JNIEnv* env, const JavaParamRef<jobject>& obj) {
  std::move(on_cancel_).Run();
}
