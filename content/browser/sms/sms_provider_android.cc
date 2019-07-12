// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string>

#include "base/bind.h"
#include "content/browser/sms/sms_provider_android.h"
#include "url/gurl.h"
#include "url/origin.h"

#include "content/public/android/content_jni_headers/SmsReceiver_jni.h"

using base::android::AttachCurrentThread;
using base::android::ConvertJavaStringToUTF8;
using base::android::ScopedJavaLocalRef;

namespace content {

SmsProviderAndroid::SmsProviderAndroid() : SmsProvider() {
  // This class is constructed a single time whenever the
  // first web page uses the SMS Retriever API to wait for
  // SMSes.
  JNIEnv* env = AttachCurrentThread();
  j_sms_receiver_.Reset(
      Java_SmsReceiver_create(env, reinterpret_cast<intptr_t>(this)));
}

SmsProviderAndroid::~SmsProviderAndroid() {
  JNIEnv* env = AttachCurrentThread();
  Java_SmsReceiver_destroy(env, j_sms_receiver_);
}

void SmsProviderAndroid::Retrieve() {
  JNIEnv* env = AttachCurrentThread();

  Java_SmsReceiver_listen(env, j_sms_receiver_);
}

void SmsProviderAndroid::OnReceive(
    JNIEnv* env,
    const base::android::JavaParamRef<jobject>& obj,
    jstring message) {
  std::string sms = ConvertJavaStringToUTF8(env, message);
  NotifyReceive(sms);
}

void SmsProviderAndroid::OnTimeout(
    JNIEnv* env,
    const base::android::JavaParamRef<jobject>& obj) {
}

}  // namespace content
