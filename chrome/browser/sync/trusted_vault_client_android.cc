// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/sync/trusted_vault_client_android.h"

#include "base/android/jni_android.h"
#include "base/android/jni_array.h"
#include "base/android/jni_string.h"
#include "base/android/scoped_java_ref.h"
#include "base/callback.h"
#include "base/logging.h"
#include "chrome/android/chrome_jni_headers/TrustedVaultClient_jni.h"

TrustedVaultClientAndroid::TrustedVaultClientAndroid() = default;

TrustedVaultClientAndroid::~TrustedVaultClientAndroid() = default;

void TrustedVaultClientAndroid::FetchKeys(
    const std::string& gaia_id,
    base::OnceCallback<void(const std::vector<std::string>&)> cb) {
  JNIEnv* const env = base::android::AttachCurrentThread();
  const base::android::ScopedJavaLocalRef<jstring> java_gaia_id =
      base::android::ConvertUTF8ToJavaString(env, gaia_id);

  // Fetch keys from the implementation in Java.
  const base::android::ScopedJavaLocalRef<jobjectArray> java_keys =
      Java_TrustedVaultClient_fetchKeys(env, java_gaia_id);
  DCHECK(java_keys);

  // Convert from Java bytes[][] to the C++ equivalent, in this case
  // std::vector<std::string>.
  // TODO(crbug.com/1027676): Avoid std::string for binary keys.
  std::vector<std::string> keys;
  JavaArrayOfByteArrayToStringVector(env, java_keys, &keys);
  std::move(cb).Run(keys);
}

void TrustedVaultClientAndroid::StoreKeys(
    const std::string& gaia_id,
    const std::vector<std::string>& keys) {
  // Not supported on Android, where keys are fetched outside the browser.
  NOTREACHED();
}
