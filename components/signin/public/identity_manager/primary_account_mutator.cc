// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/signin/public/identity_manager/primary_account_mutator.h"

#include "components/signin/public/identity_manager/account_info.h"

#if defined(OS_ANDROID)
#include "base/android/jni_string.h"
#endif

namespace signin {

PrimaryAccountMutator::PrimaryAccountMutator() {}

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
#endif

}  // namespace signin
