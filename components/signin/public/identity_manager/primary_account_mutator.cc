// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/signin/public/identity_manager/primary_account_mutator.h"

#include "components/signin/public/identity_manager/account_info.h"

#if defined(OS_ANDROID)
#include "base/android/jni_string.h"
#include "components/signin/internal/identity_manager/android/jni_headers/PrimaryAccountMutator_jni.h"
#endif

namespace signin {

PrimaryAccountMutator::PrimaryAccountMutator() {
#if defined(OS_ANDROID)
  java_primary_account_mutator_ = Java_PrimaryAccountMutator_Constructor(
      base::android::AttachCurrentThread(), reinterpret_cast<intptr_t>(this));
#endif
}

PrimaryAccountMutator::~PrimaryAccountMutator() {}

#if defined(OS_ANDROID)
bool PrimaryAccountMutator::SetPrimaryAccount(
    JNIEnv* env,
    const base::android::JavaParamRef<jobject>& account_id) {
  return SetPrimaryAccount(ConvertFromJavaCoreAccountId(env, account_id));
}

bool PrimaryAccountMutator::ClearPrimaryAccount(JNIEnv* env,
                                                jint action,
                                                jint source_metric,
                                                jint delete_metric) {
  return ClearPrimaryAccount(
      ClearAccountsAction::kDefault,
      static_cast<signin_metrics::ProfileSignout>(source_metric),
      static_cast<signin_metrics::SignoutDelete>(delete_metric));
}

base::android::ScopedJavaLocalRef<jobject>
PrimaryAccountMutator::GetJavaObject() {
  DCHECK(java_primary_account_mutator_);
  return base::android::ScopedJavaLocalRef<jobject>(
      java_primary_account_mutator_);
}
#endif

}  // namespace signin
