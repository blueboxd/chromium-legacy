// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/payments/content/android/payment_handler_host.h"

#include "base/android/jni_android.h"
#include "base/android/jni_array.h"
#include "base/android/jni_string.h"
#include "components/payments/content/android/byte_buffer_helper.h"
#include "components/payments/content/android/jni_headers/PaymentHandlerHost_jni.h"
#include "content/public/browser/web_contents.h"
#include "third_party/blink/public/mojom/payments/payment_handler_host.mojom.h"

namespace payments {
namespace android {

// static
jlong JNI_PaymentHandlerHost_Init(
    JNIEnv* env,
    const base::android::JavaParamRef<jobject>& web_contents,
    const base::android::JavaParamRef<jobject>& delegate) {
  return reinterpret_cast<intptr_t>(
      new PaymentHandlerHost(web_contents, delegate));
}

PaymentHandlerHost::PaymentHandlerHost(
    const base::android::JavaParamRef<jobject>& web_contents,
    const base::android::JavaParamRef<jobject>& delegate)
    : delegate_(delegate),
      payment_handler_host_(
          content::WebContents::FromJavaWebContents(web_contents),
          /*delegate=*/this) {}

PaymentHandlerHost::~PaymentHandlerHost() {}

jboolean PaymentHandlerHost::IsWaitingForPaymentDetailsUpdate(
    JNIEnv* env) const {
  return payment_handler_host_.is_waiting_for_payment_details_update();
}

jlong PaymentHandlerHost::GetNativePaymentHandlerHost(JNIEnv* env) {
  return reinterpret_cast<intptr_t>(&payment_handler_host_);
}

void PaymentHandlerHost::Destroy(JNIEnv* env) {
  delete this;
}

void PaymentHandlerHost::UpdateWith(
    JNIEnv* env,
    const base::android::JavaParamRef<jobject>& response_buffer) {
  mojom::PaymentRequestDetailsUpdatePtr response;
  bool success = mojom::PaymentRequestDetailsUpdate::Deserialize(
      std::move(JavaByteBufferToNativeByteVector(env, response_buffer)),
      &response);
  DCHECK(success);
  payment_handler_host_.UpdateWith(std::move(response));
}

void PaymentHandlerHost::OnPaymentDetailsNotUpdated(JNIEnv* env) {
  payment_handler_host_.OnPaymentDetailsNotUpdated();
}

bool PaymentHandlerHost::ChangePaymentMethod(
    const std::string& method_name,
    const std::string& stringified_data) {
  JNIEnv* env = base::android::AttachCurrentThread();
  return Java_PaymentHandlerHostDelegate_changePaymentMethodFromPaymentHandler(
      env, delegate_, base::android::ConvertUTF8ToJavaString(env, method_name),
      base::android::ConvertUTF8ToJavaString(env, stringified_data));
}

bool PaymentHandlerHost::ChangeShippingOption(
    const std::string& shipping_option_id) {
  JNIEnv* env = base::android::AttachCurrentThread();
  return Java_PaymentHandlerHostDelegate_changeShippingOptionFromPaymentHandler(
      env, delegate_,
      base::android::ConvertUTF8ToJavaString(env, shipping_option_id));
}

bool PaymentHandlerHost::ChangeShippingAddress(
    mojom::PaymentAddressPtr shipping_address) {
  JNIEnv* env = base::android::AttachCurrentThread();
  base::android::ScopedJavaLocalRef<jobject> jshipping_address =
      Java_PaymentHandlerHost_createShippingAddress(
          env,
          base::android::ConvertUTF8ToJavaString(env,
                                                 shipping_address->country),
          base::android::ToJavaArrayOfStrings(env,
                                              shipping_address->address_line),
          base::android::ConvertUTF8ToJavaString(env, shipping_address->region),
          base::android::ConvertUTF8ToJavaString(env, shipping_address->city),
          base::android::ConvertUTF8ToJavaString(
              env, shipping_address->dependent_locality),
          base::android::ConvertUTF8ToJavaString(env,
                                                 shipping_address->postal_code),
          base::android::ConvertUTF8ToJavaString(
              env, shipping_address->sorting_code),
          base::android::ConvertUTF8ToJavaString(
              env, shipping_address->organization),
          base::android::ConvertUTF8ToJavaString(env,
                                                 shipping_address->recipient),
          base::android::ConvertUTF8ToJavaString(env, shipping_address->phone));
  return Java_PaymentHandlerHostDelegate_changeShippingAddressFromPaymentHandler(
      env, delegate_, jshipping_address);
}

}  // namespace android
}  // namespace payments
