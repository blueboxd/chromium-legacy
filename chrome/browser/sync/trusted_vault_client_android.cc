// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/sync/trusted_vault_client_android.h"

#include <utility>

#include "base/android/jni_android.h"
#include "base/logging.h"
#include "chrome/android/chrome_jni_headers/TrustedVaultClient_jni.h"
#include "content/public/browser/browser_thread.h"

TrustedVaultClientAndroid::OngoingFetchKeys::OngoingFetchKeys(
    const std::string& gaia_id,
    base::OnceCallback<void(const std::vector<std::string>&)> callback)
    : gaia_id(gaia_id), callback(std::move(callback)) {}

TrustedVaultClientAndroid::OngoingFetchKeys::~OngoingFetchKeys() = default;

TrustedVaultClientAndroid::TrustedVaultClientAndroid() {
  JNIEnv* const env = base::android::AttachCurrentThread();
  Java_TrustedVaultClient_registerNative(env, reinterpret_cast<intptr_t>(this));
}

TrustedVaultClientAndroid::~TrustedVaultClientAndroid() {
  JNIEnv* const env = base::android::AttachCurrentThread();
  Java_TrustedVaultClient_unregisterNative(env,
                                           reinterpret_cast<intptr_t>(this));
}

void TrustedVaultClientAndroid::FetchKeysCompleted(
    JNIEnv* env,
    const base::android::JavaParamRef<jstring>& gaia_id,
    const base::android::JavaParamRef<jobjectArray>& keys) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  DCHECK(ongoing_fetch_keys_) << "No ongoing FetchKeys() request";
  DCHECK_EQ(ongoing_fetch_keys_->gaia_id,
            base::android::ConvertJavaStringToUTF8(env, gaia_id))
      << "User mismatch in FetchKeys() response";

  // Make a copy of the callback and reset |ongoing_fetch_keys_| before invoking
  // the callback, in case it has side effects.
  base::OnceCallback<void(const std::vector<std::string>&)> cb =
      std::move(ongoing_fetch_keys_->callback);
  ongoing_fetch_keys_.reset();

  // Convert from Java bytes[][] to the C++ equivalent, in this case
  // std::vector<std::string>.
  // TODO(crbug.com/1027676): Avoid std::string for binary keys.
  std::vector<std::string> converted_keys;
  JavaArrayOfByteArrayToStringVector(env, keys, &converted_keys);
  std::move(cb).Run(converted_keys);
}

std::unique_ptr<TrustedVaultClientAndroid::Subscription>
TrustedVaultClientAndroid::AddKeysChangedObserver(
    const base::RepeatingClosure& cb) {
  return observer_list_.Add(cb);
}

void TrustedVaultClientAndroid::FetchKeys(
    const std::string& gaia_id,
    base::OnceCallback<void(const std::vector<std::string>&)> cb) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  DCHECK(!ongoing_fetch_keys_)
      << "Only one FetchKeys() request is allowed at any time";

  // Store for later completion when Java invokes FetchKeysCompleted().
  ongoing_fetch_keys_ =
      std::make_unique<OngoingFetchKeys>(gaia_id, std::move(cb));

  JNIEnv* const env = base::android::AttachCurrentThread();
  const base::android::ScopedJavaLocalRef<jstring> java_gaia_id =
      base::android::ConvertUTF8ToJavaString(env, gaia_id);

  // Trigger the fetching keys from the implementation in Java, which will
  // eventually call FetchKeysCompleted().
  Java_TrustedVaultClient_fetchKeys(env, reinterpret_cast<intptr_t>(this),
                                    java_gaia_id);
}

void TrustedVaultClientAndroid::StoreKeys(
    const std::string& gaia_id,
    const std::vector<std::string>& keys) {
  // Not supported on Android, where keys are fetched outside the browser.
  NOTREACHED();
}
