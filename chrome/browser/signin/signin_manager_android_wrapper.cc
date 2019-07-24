// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/signin/signin_manager_android_wrapper.h"

SigninManagerAndroidWrapper::SigninManagerAndroidWrapper(
    SigninClient* signin_client,
    PrefService* local_state_prefs_service,
    signin::IdentityManager* identity_manager,
    std::unique_ptr<SigninManagerDelegate> signin_manager_delegate)
    : signin_manager_android_(signin_client,
                              local_state_prefs_service,
                              identity_manager,
                              std::move(signin_manager_delegate)) {}

SigninManagerAndroidWrapper::~SigninManagerAndroidWrapper() {}

void SigninManagerAndroidWrapper::Shutdown() {
  signin_manager_android_.Shutdown();
}

base::android::ScopedJavaLocalRef<jobject>
SigninManagerAndroidWrapper::GetJavaObject() {
  return signin_manager_android_.GetJavaObject();
}
