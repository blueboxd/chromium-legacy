// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ANDROID_PASSWORD_EDITING_BRIDGE_H_
#define CHROME_BROWSER_ANDROID_PASSWORD_EDITING_BRIDGE_H_

#include <stddef.h>

#include <memory>

#include "base/android/jni_weak_ref.h"
#include "base/macros.h"
#include "chrome/browser/android/password_change_delegate.h"

// A bridge that allows communication between Android UI and the native
// side. It can be used to launch the password editing activity from the
// native side or when the credential change made in the UI needs to
// arrive to the native side to be saved.
class PasswordEditingBridge {
 public:
  PasswordEditingBridge();
  ~PasswordEditingBridge();

  void SetDelegate(
      std::unique_ptr<PasswordChangeDelegate> password_change_delegate);
  // This is called when the view is destroyed and must be called because
  // it's the only way to destroy the bridge and the delegate with it.
  void Destroy(JNIEnv* env, const base::android::JavaParamRef<jobject>& obj);

 private:
  std::unique_ptr<PasswordChangeDelegate> password_change_delegate_;

  DISALLOW_COPY_AND_ASSIGN(PasswordEditingBridge);
};

#endif  // CHROME_BROWSER_ANDROID_PASSWORD_EDITING_BRIDGE_H_
