// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/android/sms/sms_dialog_android.h"

#include "base/android/jni_android.h"
#include "base/android/jni_string.h"
#include "chrome/android/chrome_jni_headers/SmsReceiverDialog_jni.h"
#include "components/url_formatter/elide_url.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/web_contents.h"
#include "ui/android/window_android.h"
#include "url/origin.h"

using base::android::AttachCurrentThread;
using base::android::JavaParamRef;
using base::android::ScopedJavaLocalRef;

SmsDialogAndroid::SmsDialogAndroid(const url::Origin& origin) {
  JNIEnv* env = AttachCurrentThread();
  base::android::ScopedJavaLocalRef<jstring> origin_string =
      base::android::ConvertUTF16ToJavaString(
          env, url_formatter::FormatOriginForSecurityDisplay(
                   origin, url_formatter::SchemeDisplay::OMIT_HTTP_AND_HTTPS));
  java_dialog_.Reset(Java_SmsReceiverDialog_create(
      env, reinterpret_cast<intptr_t>(this), origin_string));
}

SmsDialogAndroid::~SmsDialogAndroid() {
  Java_SmsReceiverDialog_destroy(AttachCurrentThread(), java_dialog_);
}

void SmsDialogAndroid::Open(content::RenderFrameHost* host,
                            EventHandler handler) {
  handler_ = std::move(handler);

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

void SmsDialogAndroid::SmsReceived() {
  Java_SmsReceiverDialog_smsReceived(AttachCurrentThread(), java_dialog_);
}

void SmsDialogAndroid::SmsTimeout() {
  Java_SmsReceiverDialog_smsTimeout(AttachCurrentThread(), java_dialog_);
}

void SmsDialogAndroid::OnEvent(JNIEnv* env, jint event_type) {
  std::move(handler_).Run(static_cast<Event>(event_type));
}
