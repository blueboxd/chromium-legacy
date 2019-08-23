// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/android/autofill_assistant/assistant_payment_request_delegate.h"

#include <memory>
#include <utility>

#include "base/android/jni_string.h"
#include "chrome/android/features/autofill_assistant/jni_headers/AssistantPaymentRequestNativeDelegate_jni.h"
#include "chrome/browser/android/autofill_assistant/ui_controller_android.h"
#include "chrome/browser/autofill/android/personal_data_manager_android.h"
#include "chrome/browser/autofill/personal_data_manager_factory.h"
#include "chrome/browser/profiles/profile_manager.h"

using base::android::AttachCurrentThread;
using base::android::JavaParamRef;

namespace autofill_assistant {

AssistantPaymentRequestDelegate::AssistantPaymentRequestDelegate(
    UiControllerAndroid* ui_controller)
    : ui_controller_(ui_controller) {
  java_assistant_payment_request_delegate_ =
      Java_AssistantPaymentRequestNativeDelegate_create(
          AttachCurrentThread(), reinterpret_cast<intptr_t>(this));
}

AssistantPaymentRequestDelegate::~AssistantPaymentRequestDelegate() {
  Java_AssistantPaymentRequestNativeDelegate_clearNativePtr(
      AttachCurrentThread(), java_assistant_payment_request_delegate_);
}

void AssistantPaymentRequestDelegate::OnContactInfoChanged(
    JNIEnv* env,
    const base::android::JavaParamRef<jobject>& jcaller,
    const base::android::JavaParamRef<jstring>& jpayer_name,
    const base::android::JavaParamRef<jstring>& jpayer_phone,
    const base::android::JavaParamRef<jstring>& jpayer_email) {
  std::string name;
  std::string phone;
  std::string email;

  if (jpayer_name) {
    base::android::ConvertJavaStringToUTF8(env, jpayer_name, &name);
  }

  if (jpayer_phone) {
    base::android::ConvertJavaStringToUTF8(env, jpayer_phone, &phone);
  }

  if (jpayer_email) {
    base::android::ConvertJavaStringToUTF8(env, jpayer_email, &email);
  }

  ui_controller_->OnContactInfoChanged(name, phone, email);
}

void AssistantPaymentRequestDelegate::OnShippingAddressChanged(
    JNIEnv* env,
    const base::android::JavaParamRef<jobject>& jcaller,
    const base::android::JavaParamRef<jobject>& jaddress) {
  if (!jaddress) {
    ui_controller_->OnShippingAddressChanged(nullptr);
    return;
  }

  auto shipping_address = std::make_unique<autofill::AutofillProfile>();
  autofill::PersonalDataManagerAndroid::PopulateNativeProfileFromJava(
      jaddress, env, shipping_address.get());
  ui_controller_->OnShippingAddressChanged(std::move(shipping_address));
}

void AssistantPaymentRequestDelegate::OnCreditCardChanged(
    JNIEnv* env,
    const base::android::JavaParamRef<jobject>& jcaller,
    const base::android::JavaParamRef<jobject>& jcard) {
  if (!jcard) {
    ui_controller_->OnCreditCardChanged(nullptr);
    return;
  }

  auto card = std::make_unique<autofill::CreditCard>();
  autofill::PersonalDataManagerAndroid::PopulateNativeCreditCardFromJava(
      jcard, env, card.get());

  auto guid = card->billing_address_id();
  if (!guid.empty()) {
    autofill::AutofillProfile* profile =
        autofill::PersonalDataManagerFactory::GetForProfile(
            ProfileManager::GetLastUsedProfile())
            ->GetProfileByGUID(guid);
    if (profile != nullptr) {
      auto billing_address =
          std::make_unique<autofill::AutofillProfile>(*profile);
      ui_controller_->OnBillingAddressChanged(std::move(billing_address));
    }
  }

  ui_controller_->OnCreditCardChanged(std::move(card));
}

void AssistantPaymentRequestDelegate::OnTermsAndConditionsChanged(
    JNIEnv* env,
    const base::android::JavaParamRef<jobject>& jcaller,
    jint state) {
  ui_controller_->OnTermsAndConditionsChanged(
      static_cast<TermsAndConditionsState>(state));
}

void AssistantPaymentRequestDelegate::OnTermsAndConditionsLinkClicked(
    JNIEnv* env,
    const base::android::JavaParamRef<jobject>& jcaller,
    jint link) {
  ui_controller_->OnTermsAndConditionsLinkClicked(link);
}

base::android::ScopedJavaGlobalRef<jobject>
AssistantPaymentRequestDelegate::GetJavaObject() {
  return java_assistant_payment_request_delegate_;
}

}  // namespace autofill_assistant
