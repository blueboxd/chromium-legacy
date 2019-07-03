// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/signin/signin_manager_android_wrapper.h"

SigninManagerAndroidWrapper::SigninManagerAndroidWrapper(
    Profile* profile,
    identity::IdentityManager* identity_manager)
    : signin_manager_android_(profile, identity_manager) {}

SigninManagerAndroidWrapper::~SigninManagerAndroidWrapper() {}

void SigninManagerAndroidWrapper::Shutdown() {
  signin_manager_android_.Shutdown();
}

base::android::ScopedJavaLocalRef<jobject>
SigninManagerAndroidWrapper::GetJavaObject() {
  return signin_manager_android_.GetJavaObject();
}
