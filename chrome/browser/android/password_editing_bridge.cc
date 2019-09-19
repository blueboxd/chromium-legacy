// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/android/password_editing_bridge.h"

#include "base/android/jni_string.h"
#include "chrome/android/chrome_jni_headers/PasswordEditingBridge_jni.h"
#include "chrome/browser/android/password_edit_delegate.h"
#include "chrome/browser/android/password_update_delegate.h"

using base::android::ConvertUTF16ToJavaString;
using base::android::ConvertUTF8ToJavaString;
using base::android::JavaParamRef;

PasswordEditingBridge::PasswordEditingBridge() {
  java_object_.Reset(Java_PasswordEditingBridge_create(
      base::android::AttachCurrentThread(), reinterpret_cast<intptr_t>(this)));
}

PasswordEditingBridge::~PasswordEditingBridge() = default;

void PasswordEditingBridge::Destroy(JNIEnv* env,
                                    const JavaParamRef<jobject>& obj) {
  delete this;
}

void PasswordEditingBridge::LaunchPasswordEntryEditor(
    JNIEnv* env,
    const base::android::JavaParamRef<jobject>& context,
    Profile* profile,
    const autofill::PasswordForm& password_form) {
  // PasswordEditingBridge will destroy itself when the UI is gone on the Java
  // side.
  PasswordEditingBridge* password_editing_bridge = new PasswordEditingBridge();
  password_editing_bridge->password_edit_delegate_ =
      std::make_unique<PasswordUpdateDelegate>(profile, password_form);
  Java_PasswordEditingBridge_showEditingUI(
      base::android::AttachCurrentThread(),
      password_editing_bridge->java_object_, context,
      ConvertUTF8ToJavaString(env, password_form.signon_realm),
      ConvertUTF16ToJavaString(env, password_form.username_value),
      ConvertUTF16ToJavaString(env, password_form.password_value));
}

void PasswordEditingBridge::HandleEditSavedPasswordEntry(
    JNIEnv* env,
    const JavaParamRef<jobject>& object,
    const JavaParamRef<jstring>& new_username,
    const JavaParamRef<jstring>& new_password) {
  password_edit_delegate_->EditSavedPassword(
      ConvertJavaStringToUTF16(env, new_username),
      ConvertJavaStringToUTF16(env, new_password));
}
