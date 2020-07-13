// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PASSWORD_CHECK_ANDROID_INTERNAL_PASSWORD_CHECK_BRIDGE_H_
#define CHROME_BROWSER_PASSWORD_CHECK_ANDROID_INTERNAL_PASSWORD_CHECK_BRIDGE_H_

#include <jni.h>
#include "base/android/scoped_java_ref.h"

// C++ counterpart of |PasswordCheckBridge.java|. Used to mediate the
// communication between the UI and the password check logic.
class PasswordCheckBridge {
 public:
  PasswordCheckBridge();
  PasswordCheckBridge(const PasswordCheckBridge&) = delete;
  PasswordCheckBridge& operator=(const PasswordCheckBridge&) = delete;

  // Called by Java to start the password check.
  void StartCheck(JNIEnv* env);

  // Called by Java to stop the password check.
  void StopCheck(JNIEnv* env);

  // Called by Java to get the number of compromised credentials.
  jint GetCompromisedCredentialsCount(JNIEnv* env);

  // Called by Java to get the list of compromised credentials.
  void GetCompromisedCredentials(
      JNIEnv* env,
      const base::android::JavaParamRef<jobjectArray>& credentials);

  // Called by Java when the bridge is no longer needed. Destructs itself.
  void Destroy(JNIEnv* env);

 private:
  ~PasswordCheckBridge();
};

#endif  // CHROME_BROWSER_PASSWORD_CHECK_ANDROID_INTERNAL_PASSWORD_CHECK_BRIDGE_H_
