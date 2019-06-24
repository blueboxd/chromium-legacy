// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/android/autofill/card_name_fix_flow_view_android.h"

#include "chrome/android/chrome_jni_headers/AutofillNameFixFlowBridge_jni.h"
#include "chrome/browser/android/resource_mapper.h"
#include "components/autofill/core/browser/ui/payments/card_name_fix_flow_view_delegate_mobile.h"
#include "content/public/browser/web_contents.h"
#include "ui/android/view_android.h"
#include "ui/android/window_android.h"

using base::android::JavaParamRef;
using base::android::ScopedJavaLocalRef;

namespace autofill {

CardNameFixFlowViewAndroid::CardNameFixFlowViewAndroid(
    std::unique_ptr<CardNameFixFlowViewDelegateMobile> delegate,
    content::WebContents* web_contents)
    : delegate_(std::move(delegate)), web_contents_(web_contents) {}

CardNameFixFlowViewAndroid::~CardNameFixFlowViewAndroid() {
  JNIEnv* env = base::android::AttachCurrentThread();
  Java_AutofillNameFixFlowBridge_dismiss(env, java_object_);
}

void CardNameFixFlowViewAndroid::Show() {
  JNIEnv* env = base::android::AttachCurrentThread();
  ui::ViewAndroid* view_android = web_contents_->GetNativeView();

  ScopedJavaLocalRef<jstring> dialog_title =
      base::android::ConvertUTF16ToJavaString(env, delegate_->GetTitleText());

  ScopedJavaLocalRef<jstring> inferred_name =
      base::android::ConvertUTF16ToJavaString(
          env, delegate_->GetInferredCardHolderName());

  ScopedJavaLocalRef<jstring> confirm = base::android::ConvertUTF16ToJavaString(
      env, delegate_->GetSaveButtonLabel());

  java_object_.Reset(Java_AutofillNameFixFlowBridge_create(
      env, reinterpret_cast<intptr_t>(this), dialog_title, inferred_name,
      confirm, ResourceMapper::MapFromChromiumId(delegate_->GetIconId()),
      view_android->GetWindowAndroid()->GetJavaObject()));

  Java_AutofillNameFixFlowBridge_show(
      env, java_object_, view_android->GetWindowAndroid()->GetJavaObject());
  delegate_->Shown();
}

void CardNameFixFlowViewAndroid::OnUserAccept(
    JNIEnv* env,
    const JavaParamRef<jobject>& obj,
    const JavaParamRef<jstring>& name) {
  delegate_->Accept(base::android::ConvertJavaStringToUTF16(env, name));
  Java_AutofillNameFixFlowBridge_dismiss(env, java_object_);
}

void CardNameFixFlowViewAndroid::PromptDismissed(
    JNIEnv* env,
    const JavaParamRef<jobject>& obj) {
  delegate_->Dismissed();
  delete this;
}

}  // namespace autofill
