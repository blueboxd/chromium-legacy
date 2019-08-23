// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ANDROID_AUTOFILL_ASSISTANT_ASSISTANT_PAYMENT_REQUEST_DELEGATE_H_
#define CHROME_BROWSER_ANDROID_AUTOFILL_ASSISTANT_ASSISTANT_PAYMENT_REQUEST_DELEGATE_H_

#include "base/android/scoped_java_ref.h"

namespace autofill_assistant {
class UiControllerAndroid;
// Delegate class for the payment_request, to react on clicks on its chips.
class AssistantPaymentRequestDelegate {
 public:
  explicit AssistantPaymentRequestDelegate(UiControllerAndroid* ui_controller);
  ~AssistantPaymentRequestDelegate();

  void OnContactInfoChanged(
      JNIEnv* env,
      const base::android::JavaParamRef<jobject>& jcaller,
      const base::android::JavaParamRef<jstring>& jpayer_name,
      const base::android::JavaParamRef<jstring>& jpayer_phone,
      const base::android::JavaParamRef<jstring>& jpayer_email);

  void OnShippingAddressChanged(
      JNIEnv* env,
      const base::android::JavaParamRef<jobject>& jcaller,
      const base::android::JavaParamRef<jobject>& jaddress);

  void OnCreditCardChanged(JNIEnv* env,
                           const base::android::JavaParamRef<jobject>& jcaller,
                           const base::android::JavaParamRef<jobject>& jcard);

  void OnTermsAndConditionsChanged(
      JNIEnv* env,
      const base::android::JavaParamRef<jobject>& jcaller,
      jint state);

  void OnTermsAndConditionsLinkClicked(
      JNIEnv* env,
      const base::android::JavaParamRef<jobject>& jcaller,
      jint link);

  base::android::ScopedJavaGlobalRef<jobject> GetJavaObject();

 private:
  UiControllerAndroid* ui_controller_;

  // Java-side AssistantPaymentRequestDelegate object.
  base::android::ScopedJavaGlobalRef<jobject>
      java_assistant_payment_request_delegate_;
};
}  // namespace autofill_assistant

#endif  // CHROME_BROWSER_ANDROID_AUTOFILL_ASSISTANT_ASSISTANT_PAYMENT_REQUEST_DELEGATE_H_
