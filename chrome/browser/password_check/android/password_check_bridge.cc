// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/password_check/android/password_check_bridge.h"

#include <jni.h>

#include "chrome/browser/password_check/android/internal/jni_headers/PasswordCheckBridge_jni.h"

static jlong JNI_PasswordCheckBridge_Create(
    JNIEnv* env,
    const base::android::JavaParamRef<jobject>& java_bridge) {
  return reinterpret_cast<intptr_t>(new PasswordCheckBridge(java_bridge));
}

PasswordCheckBridge::PasswordCheckBridge(
    const base::android::JavaParamRef<jobject>& java_bridge)
    : java_bridge_(java_bridge) {}
PasswordCheckBridge::~PasswordCheckBridge() = default;

void PasswordCheckBridge::StartCheck(JNIEnv* env) {
  // TODO(crbug.com/1102025): implement this.
}

void PasswordCheckBridge::StopCheck(JNIEnv* env) {
  // TODO(crbug.com/1102025): implement this.
}

jint PasswordCheckBridge::GetCompromisedCredentialsCount(JNIEnv* env) {
  // TODO(crbug.com/1102025): implement this.
  return 0;
}

jint PasswordCheckBridge::GetSavedPasswordsCount(JNIEnv* env) {
  // TODO(crbug.com/1102025): implement this.
  return 0;
}

void PasswordCheckBridge::GetCompromisedCredentials(
    JNIEnv* env,
    const base::android::JavaParamRef<jobjectArray>& credentials) {
  // TODO(crbug.com/1102025): implement this.
}

void PasswordCheckBridge::RemoveCredential(
    JNIEnv* env,
    const base::android::JavaParamRef<jobject>& credential) {
  // TODO(crbug.com/1108358): implement this.
}

void PasswordCheckBridge::Destroy(JNIEnv* env) {
  delete this;
}

void PasswordCheckBridge::OnPasswordCheckStatusChanged(
    password_manager::PasswordCheckUIStatus status) {
  Java_PasswordCheckBridge_onPasswordCheckStatusChanged(
      base::android::AttachCurrentThread(), java_bridge_,
      static_cast<int>(status));
}
