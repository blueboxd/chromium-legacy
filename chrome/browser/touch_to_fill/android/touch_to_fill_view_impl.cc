// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/touch_to_fill/android/touch_to_fill_view_impl.h"

#include <memory>

#include "base/android/jni_android.h"
#include "base/android/jni_string.h"
#include "base/macros.h"
#include "base/strings/string_piece.h"
#include "chrome/browser/password_manager/touch_to_fill_controller.h"
#include "chrome/browser/touch_to_fill/android/jni_headers/Credential_jni.h"
#include "chrome/browser/touch_to_fill/android/jni_headers/TouchToFillBridge_jni.h"
#include "components/password_manager/core/browser/origin_credential_store.h"
#include "ui/android/view_android.h"
#include "ui/android/window_android.h"
#include "url/gurl.h"

using base::android::AttachCurrentThread;
using base::android::ConvertJavaStringToUTF16;
using base::android::ConvertJavaStringToUTF8;
using base::android::ConvertUTF16ToJavaString;
using base::android::ConvertUTF8ToJavaString;
using base::android::JavaParamRef;
using password_manager::CredentialPair;

namespace {

CredentialPair ConvertJavaCredential(JNIEnv* env,
                                     const JavaParamRef<jobject>& credential) {
  return CredentialPair(
      ConvertJavaStringToUTF16(env,
                               Java_Credential_getUsername(env, credential)),
      ConvertJavaStringToUTF16(env,
                               Java_Credential_getPassword(env, credential)),
      GURL(ConvertJavaStringToUTF8(
          env, Java_Credential_getOriginUrl(env, credential))),
      Java_Credential_isPublicSuffixMatch(env, credential));
}

}  // namespace

TouchToFillViewImpl::TouchToFillViewImpl(TouchToFillController* controller)
    : controller_(controller) {
  java_object_ = Java_TouchToFillBridge_create(
      AttachCurrentThread(), reinterpret_cast<intptr_t>(this),
      controller_->GetNativeView()->GetWindowAndroid()->GetJavaObject());
}

TouchToFillViewImpl::~TouchToFillViewImpl() {
  Java_TouchToFillBridge_destroy(AttachCurrentThread(), java_object_);
}

void TouchToFillViewImpl::Show(
    base::StringPiece16 formatted_url,
    base::span<const password_manager::CredentialPair> credentials,
    ShowCallback callback) {
  callback_ = std::move(callback);

  // Serialize the |credentials| span into a Java array and instruct the bridge
  // to show it together with |formatted_url| to the user.
  JNIEnv* env = AttachCurrentThread();
  auto credential_array =
      Java_TouchToFillBridge_createCredentialArray(env, credentials.size());
  for (size_t i = 0; i < credentials.size(); ++i) {
    const auto& credential = credentials[i];
    Java_TouchToFillBridge_insertCredential(
        env, credential_array, i,
        ConvertUTF16ToJavaString(env, credential.username),
        ConvertUTF16ToJavaString(env, credential.password),
        ConvertUTF8ToJavaString(env, credential.origin_url.spec()),
        credential.is_public_suffix_match);
  }

  Java_TouchToFillBridge_showCredentials(
      env, java_object_, ConvertUTF16ToJavaString(env, formatted_url),
      credential_array);
}

void TouchToFillViewImpl::OnDismiss() {
  // TODO(crbug.com/957532): Implement.
}

void TouchToFillViewImpl::OnCredentialSelected(
    JNIEnv* env,
    const JavaParamRef<jobject>& credential) {
  std::move(callback_).Run(ConvertJavaCredential(env, credential));
}

void TouchToFillViewImpl::OnDismiss(JNIEnv* env) {
  OnDismiss();
}
