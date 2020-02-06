// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/android/preferences/cookie_controls_service_bridge.h"

#include <memory>
#include "chrome/android/chrome_jni_headers/CookieControlsServiceBridge_jni.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_android.h"
#include "chrome/browser/ui/cookie_controls/cookie_controls_service.h"
#include "chrome/browser/ui/cookie_controls/cookie_controls_service_factory.h"

using base::android::JavaParamRef;

CookieControlsServiceBridge::CookieControlsServiceBridge(
    JNIEnv* env,
    const JavaParamRef<jobject>& obj,
    const base::android::JavaParamRef<jobject>& jprofile)
    : jobject_(obj) {
  Profile* profile = ProfileAndroid::FromProfileAndroid(jprofile);
  service_ = CookieControlsServiceFactory::GetForProfile(profile);
  service_->AddObserver(this);
}

void CookieControlsServiceBridge::HandleCookieControlsToggleChanged(
    JNIEnv* env,
    jboolean checked) {
  service_->HandleCookieControlsToggleChanged(checked);
}

void CookieControlsServiceBridge::SendCookieControlsUIChanges() {
  bool checked = service_->GetToggleCheckedValue();
  bool enforced = service_->ShouldEnforceCookieControls();
  JNIEnv* env = base::android::AttachCurrentThread();
  Java_CookieControlsServiceBridge_sendCookieControlsUIChanges(
      env, jobject_, checked, enforced);
}

void CookieControlsServiceBridge::OnThirdPartyCookieBlockingPrefChanged() {
  SendCookieControlsUIChanges();
}

void CookieControlsServiceBridge::OnThirdPartyCookieBlockingPolicyChanged() {
  SendCookieControlsUIChanges();
}

CookieControlsServiceBridge::~CookieControlsServiceBridge() = default;

void CookieControlsServiceBridge::Destroy(JNIEnv* env,
                                          const JavaParamRef<jobject>& obj) {
  delete this;
}

static jlong JNI_CookieControlsServiceBridge_Init(
    JNIEnv* env,
    const JavaParamRef<jobject>& obj,
    const base::android::JavaParamRef<jobject>& jprofile) {
  return reinterpret_cast<intptr_t>(
      new CookieControlsServiceBridge(env, obj, jprofile));
}
