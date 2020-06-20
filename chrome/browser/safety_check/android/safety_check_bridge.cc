// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/safety_check/android/safety_check_bridge.h"

#include <jni.h>

#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/safety_check/android/jni_headers/SafetyCheckBridge_jni.h"
#include "components/safety_check/safety_check.h"

static jlong JNI_SafetyCheckBridge_Init(
    JNIEnv* env,
    const base::android::JavaParamRef<jobject>& obj,
    const base::android::JavaParamRef<jobject>& j_safety_check_observer) {
  return reinterpret_cast<intptr_t>(
      new SafetyCheckBridge(env, j_safety_check_observer));
}

SafetyCheckBridge::SafetyCheckBridge(
    JNIEnv* env,
    const base::android::JavaParamRef<jobject>& j_safety_check_observer)
    : pref_service_(ProfileManager::GetActiveUserProfile()
                        ->GetOriginalProfile()
                        ->GetPrefs()),
      j_safety_check_observer_(j_safety_check_observer) {
  safety_check_.reset(new safety_check::SafetyCheck(this));
  password_check_controller_.reset(new BulkLeakCheckControllerAndroid());
}

void SafetyCheckBridge::Destroy(
    JNIEnv* env,
    const base::android::JavaParamRef<jobject>& obj) {
  password_check_controller_.reset();
  safety_check_.reset();
  delete this;
}

void SafetyCheckBridge::CheckSafeBrowsing(
    JNIEnv* env,
    const base::android::JavaParamRef<jobject>& obj) {
  safety_check_->CheckSafeBrowsing(pref_service_);
}

void SafetyCheckBridge::CheckPasswords(
    JNIEnv* env,
    const base::android::JavaParamRef<jobject>& obj) {
  password_check_controller_->AddObserver(this);
  password_check_controller_->StartPasswordCheck();
}

int SafetyCheckBridge::GetNumberOfPasswordLeaksFromLastCheck(
    JNIEnv* env,
    const base::android::JavaParamRef<jobject>& obj) {
  return password_check_controller_->GetNumberOfLeaksFromLastCheck();
}

bool SafetyCheckBridge::SavedPasswordsExist(
    JNIEnv* env,
    const base::android::JavaParamRef<jobject>& obj) {
  return password_check_controller_->GetNumberOfSavedPasswords() != 0;
}

void SafetyCheckBridge::StopObservingPasswordsCheck(
    JNIEnv* env,
    const base::android::JavaParamRef<jobject>& obj) {
  password_check_controller_->RemoveObserver(this);
}

void SafetyCheckBridge::OnSafeBrowsingCheckResult(
    safety_check::SafetyCheck::SafeBrowsingStatus status) {
  JNIEnv* env = base::android::AttachCurrentThread();
  Java_SafetyCheckCommonObserver_onSafeBrowsingCheckResult(
      env, j_safety_check_observer_, static_cast<int>(status));
}

void SafetyCheckBridge::OnStateChanged(
    password_manager::BulkLeakCheckService::State state) {
  JNIEnv* env = base::android::AttachCurrentThread();
  Java_SafetyCheckCommonObserver_onPasswordCheckStateChange(
      env, j_safety_check_observer_, static_cast<int>(state));
}

void SafetyCheckBridge::OnCredentialDone(
    const password_manager::LeakCheckCredential& credential,
    password_manager::IsLeaked is_leaked,
    SafetyCheckBridge::DoneCount credentials_checked,
    SafetyCheckBridge::TotalCount total_to_check) {
  JNIEnv* env = base::android::AttachCurrentThread();
  Java_SafetyCheckCommonObserver_onPasswordCheckCredentialDone(
      env, j_safety_check_observer_, credentials_checked.value(),
      total_to_check.value());
}

SafetyCheckBridge::~SafetyCheckBridge() = default;
