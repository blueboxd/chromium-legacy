// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SYNC_TRUSTED_VAULT_CLIENT_ANDROID_H_
#define CHROME_BROWSER_SYNC_TRUSTED_VAULT_CLIENT_ANDROID_H_

#include <memory>
#include <string>
#include <vector>

#include "base/android/jni_array.h"
#include "base/android/jni_string.h"
#include "base/android/scoped_java_ref.h"
#include "base/callback.h"
#include "components/sync/driver/trusted_vault_client.h"

// JNI bridge for a Java implementation of the TrustedVaultClient interface,
// used on Android.
//
// This class must be accessed from the UI thread.
class TrustedVaultClientAndroid : public syncer::TrustedVaultClient {
 public:
  TrustedVaultClientAndroid();
  ~TrustedVaultClientAndroid() override;

  TrustedVaultClientAndroid(const TrustedVaultClientAndroid&) = delete;
  TrustedVaultClientAndroid& operator=(const TrustedVaultClientAndroid&) =
      delete;

  // Called from Java to notify the completion of a FetchKeys() operation
  // previously initiated from C++. This must correspond to an ongoing
  // FetchKeys() request, and |gaia_id| must match the user's ID.
  void FetchKeysCompleted(
      JNIEnv* env,
      const base::android::JavaParamRef<jstring>& gaia_id,
      const base::android::JavaParamRef<jobjectArray>& keys);

  // TrustedVaultClient implementation.
  std::unique_ptr<Subscription> AddKeysChangedObserver(
      const base::RepeatingClosure& cb) override;
  void FetchKeys(
      const std::string& gaia_id,
      base::OnceCallback<void(const std::vector<std::string>&)> cb) override;
  void StoreKeys(const std::string& gaia_id,
                 const std::vector<std::string>& keys) override;

 private:
  // Struct representing an in-flight FetchKeys() call invoked from C++.
  struct OngoingFetchKeys {
    OngoingFetchKeys(
        const std::string& gaia_id,
        base::OnceCallback<void(const std::vector<std::string>&)> callback);
    ~OngoingFetchKeys();

    const std::string gaia_id;
    base::OnceCallback<void(const std::vector<std::string>&)> callback;
  };

  // Null if no in-flight FetchKeys().
  std::unique_ptr<OngoingFetchKeys> ongoing_fetch_keys_;

  CallbackList observer_list_;
};

#endif  // CHROME_BROWSER_SYNC_TRUSTED_VAULT_CLIENT_ANDROID_H_
