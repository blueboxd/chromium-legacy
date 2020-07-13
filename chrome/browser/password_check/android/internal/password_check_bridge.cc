// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/password_check/android/internal/password_check_bridge.h"

#include <jni.h>

#include "chrome/browser/password_check/android/internal/jni_headers/PasswordCheckBridge_jni.h"

static jlong JNI_PasswordCheckBridge_Create(JNIEnv* env) {
  return reinterpret_cast<intptr_t>(new PasswordCheckBridge());
}

PasswordCheckBridge::PasswordCheckBridge() = default;
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

void PasswordCheckBridge::GetCompromisedCredentials(
    JNIEnv* env,
    const base::android::JavaParamRef<jobjectArray>& credentials) {
  // TODO(crbug.com/1102025): implement this.
}

void PasswordCheckBridge::Destroy(JNIEnv* env) {
  delete this;
}
