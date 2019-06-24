// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>
#include <utility>

#include "chrome/browser/ui/android/autofill/card_expiration_date_fix_flow_view_android.h"

#include "chrome/android/chrome_jni_headers/AutofillExpirationDateFixFlowBridge_jni.h"
#include "chrome/browser/android/resource_mapper.h"
#include "components/autofill/core/browser/ui/payments/card_expiration_date_fix_flow_view_delegate_mobile.h"
#include "content/public/browser/web_contents.h"
#include "ui/android/view_android.h"
#include "ui/android/window_android.h"

using base::android::JavaParamRef;
using base::android::ScopedJavaLocalRef;

namespace autofill {

CardExpirationDateFixFlowViewAndroid::CardExpirationDateFixFlowViewAndroid(
    std::unique_ptr<CardExpirationDateFixFlowViewDelegateMobile> delegate,
    content::WebContents* web_contents)
    : delegate_(std::move(delegate)), web_contents_(web_contents) {}

CardExpirationDateFixFlowViewAndroid::~CardExpirationDateFixFlowViewAndroid() {
  JNIEnv* env = base::android::AttachCurrentThread();
  Java_AutofillExpirationDateFixFlowBridge_dismiss(env, java_object_);
}

void CardExpirationDateFixFlowViewAndroid::Show() {
  JNIEnv* env = base::android::AttachCurrentThread();
  ui::ViewAndroid* view_android = web_contents_->GetNativeView();

  ScopedJavaLocalRef<jstring> dialog_title =
      base::android::ConvertUTF16ToJavaString(env, delegate_->GetTitleText());

  ScopedJavaLocalRef<jstring> confirm = base::android::ConvertUTF16ToJavaString(
      env, delegate_->GetSaveButtonLabel());

  ScopedJavaLocalRef<jstring> card_label =
      base::android::ConvertUTF16ToJavaString(env, delegate_->card_label());

  java_object_.Reset(Java_AutofillExpirationDateFixFlowBridge_create(
      env, reinterpret_cast<intptr_t>(this), dialog_title, confirm,
      ResourceMapper::MapFromChromiumId(delegate_->GetIconId()), card_label));

  Java_AutofillExpirationDateFixFlowBridge_show(
      env, java_object_, view_android->GetWindowAndroid()->GetJavaObject());
  delegate_->Shown();
}

void CardExpirationDateFixFlowViewAndroid::OnUserAccept(
    JNIEnv* env,
    const JavaParamRef<jobject>& obj,
    const JavaParamRef<jstring>& month,
    const JavaParamRef<jstring>& year) {
  delegate_->Accept(base::android::ConvertJavaStringToUTF16(env, month),
                    base::android::ConvertJavaStringToUTF16(env, year));
}

void CardExpirationDateFixFlowViewAndroid::PromptDismissed(
    JNIEnv* env,
    const JavaParamRef<jobject>& obj) {
  delegate_->Dismissed();
  delete this;
}

}  // namespace autofill
