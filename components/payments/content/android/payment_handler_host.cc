// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/payments/content/android/payment_handler_host.h"

#include "base/android/jni_android.h"
#include "base/android/jni_string.h"
#include "components/payments/content/android/byte_buffer_helper.h"
#include "components/payments/content/android/jni_headers/PaymentHandlerHost_jni.h"
#include "third_party/blink/public/mojom/payments/payment_handler_host.mojom.h"

namespace payments {
namespace android {

// static
jlong JNI_PaymentHandlerHost_Init(
    JNIEnv* env,
    const base::android::JavaParamRef<jobject>& delegate) {
  return reinterpret_cast<intptr_t>(new PaymentHandlerHost(delegate));
}

PaymentHandlerHost::PaymentHandlerHost(
    const base::android::JavaParamRef<jobject>& delegate)
    : delegate_(delegate), payment_handler_host_(this) {}

PaymentHandlerHost::~PaymentHandlerHost() {}

jboolean PaymentHandlerHost::IsChangingPaymentMethod(
    JNIEnv* env,
    const base::android::JavaParamRef<jobject>& j_caller) const {
  return payment_handler_host_.is_changing_payment_method();
}

jlong PaymentHandlerHost::GetNativePaymentHandlerHost(
    JNIEnv* env,
    const base::android::JavaParamRef<jobject>& caller) {
  return reinterpret_cast<intptr_t>(&payment_handler_host_);
}

void PaymentHandlerHost::Destroy(
    JNIEnv* env,
    const base::android::JavaParamRef<jobject>& caller) {
  delete this;
}

void PaymentHandlerHost::UpdateWith(
    JNIEnv* env,
    const base::android::JavaParamRef<jobject>& caller,
    const base::android::JavaParamRef<jobject>& response_buffer) {
  mojom::PaymentMethodChangeResponsePtr response;
  bool success = mojom::PaymentMethodChangeResponse::Deserialize(
      std::move(JavaByteBufferToNativeByteVector(env, response_buffer)),
      &response);
  DCHECK(success);
  payment_handler_host_.UpdateWith(std::move(response));
}

void PaymentHandlerHost::NoUpdatedPaymentDetails(
    JNIEnv* env,
    const base::android::JavaParamRef<jobject>& caller) {
  payment_handler_host_.NoUpdatedPaymentDetails();
}

bool PaymentHandlerHost::ChangePaymentMethod(
    const std::string& method_name,
    const std::string& stringified_data) {
  JNIEnv* env = base::android::AttachCurrentThread();
  return Java_PaymentHandlerHostDelegate_changePaymentMethodFromPaymentHandler(
      env, delegate_, base::android::ConvertUTF8ToJavaString(env, method_name),
      base::android::ConvertUTF8ToJavaString(env, stringified_data));
}

}  // namespace android
}  // namespace payments
