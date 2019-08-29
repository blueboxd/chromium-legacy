// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/android/password_editing_bridge.h"

#include "chrome/android/chrome_jni_headers/PasswordEditingBridge_jni.h"

using base::android::JavaParamRef;

PasswordEditingBridge::PasswordEditingBridge() {
  Java_PasswordEditingBridge_create(base::android::AttachCurrentThread(),
                                    reinterpret_cast<intptr_t>(this));
}

PasswordEditingBridge::~PasswordEditingBridge() = default;

void PasswordEditingBridge::Destroy(JNIEnv* env,
                                    const JavaParamRef<jobject>& obj) {
  delete this;
}

void PasswordEditingBridge::SetDelegate(
    std::unique_ptr<PasswordChangeDelegate> password_change_delegate) {
  password_change_delegate_ = std::move(password_change_delegate);
}
