// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PASSWORD_CHECK_ANDROID_PASSWORD_CHECK_BRIDGE_H_
#define CHROME_BROWSER_PASSWORD_CHECK_ANDROID_PASSWORD_CHECK_BRIDGE_H_

#include <jni.h>
#include "base/android/scoped_java_ref.h"
#include "chrome/browser/password_check/android/password_check_manager.h"
#include "chrome/browser/password_check/android/password_check_ui_status.h"
#include "chrome/browser/profiles/profile_manager.h"

// C++ counterpart of |PasswordCheckBridge.java|. Used to mediate the
// communication between the UI and the password check logic.
class PasswordCheckBridge : public PasswordCheckManager::Observer {
 public:
  explicit PasswordCheckBridge(
      const base::android::JavaParamRef<jobject>& java_bridge);
  ~PasswordCheckBridge() override;

  PasswordCheckBridge(const PasswordCheckBridge&) = delete;
  PasswordCheckBridge& operator=(const PasswordCheckBridge&) = delete;

  // Called by Java to start the password check.
  void StartCheck(JNIEnv* env);

  // Called by Java to stop the password check.
  void StopCheck(JNIEnv* env);

  // Called by Java to get the number of compromised credentials.
  jint GetCompromisedCredentialsCount(JNIEnv* env);

  // Called by Java to get the total number of saved passwords.
  jint GetSavedPasswordsCount(JNIEnv* env);

  // Called by Java to get the list of compromised credentials.
  void GetCompromisedCredentials(
      JNIEnv* env,
      const base::android::JavaParamRef<jobjectArray>& credentials);

  // Called by Java to remove a single compromised credentials from the password
  // store.
  void RemoveCredential(
      JNIEnv* env,
      const base::android::JavaParamRef<jobject>& credentials);

  // Called by Java when the bridge is no longer needed. Destructs itself.
  void Destroy(JNIEnv* env);

  // Called by the check manager when the saved passwords have been first loaded
  // in memory. `count` is the number of saved passwords.
  void OnSavedPasswordsFetched(int count) override;

  // Called by the check manager whenever the stored compromised credentials
  // change. `count` is the number of compromised credentials.
  void OnCompromisedCredentialsChanged(int count) override;

  // Called by the check manager when the status of the check changes.
  void OnPasswordCheckStatusChanged(
      password_manager::PasswordCheckUIStatus status) override;

 private:
  // The corresponding java object.
  base::android::ScopedJavaGlobalRef<jobject> java_bridge_;

  // Manager handling the communication with the check service, owning and
  // observing a `CompromisedCredentialManager` and a `SavedPasswordsPresenter`.
  PasswordCheckManager check_manager_{ProfileManager::GetLastUsedProfile(),
                                      this};
};

#endif  // CHROME_BROWSER_PASSWORD_CHECK_ANDROID_PASSWORD_CHECK_BRIDGE_H_
