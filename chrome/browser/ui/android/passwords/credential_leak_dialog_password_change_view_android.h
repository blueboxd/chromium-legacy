// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_ANDROID_PASSWORDS_CREDENTIAL_LEAK_DIALOG_PASSWORD_CHANGE_VIEW_ANDROID_H_
#define CHROME_BROWSER_UI_ANDROID_PASSWORDS_CREDENTIAL_LEAK_DIALOG_PASSWORD_CHANGE_VIEW_ANDROID_H_

#include <jni.h>

#include "base/android/scoped_java_ref.h"
#include "chrome/browser/ui/passwords/password_dialog_prompts.h"

namespace ui {
class WindowAndroid;
}

class CredentialLeakPasswordChangeControllerAndroid;

// TODO (crbug/1058764) Unfork credential leak dialog password change when
// prototype is done.

// Modal dialog displaying a warning for the user when an entered credential was
// detected to have been part of a leak. Communicated with its Java counterpart
// and passes responses back to the
// |CredentialLeakPasswordChangeControllerAndroid|.
class CredentialLeakDialogPasswordChangeViewAndroid {
 public:
  explicit CredentialLeakDialogPasswordChangeViewAndroid(
      CredentialLeakPasswordChangeControllerAndroid* controller);
  ~CredentialLeakDialogPasswordChangeViewAndroid();

  // Called to create and show the dialog.
  void Show(ui::WindowAndroid* window_android);

  // Called from Java via JNI.
  void Accepted(JNIEnv* env, const base::android::JavaParamRef<jobject>& obj);

  // Called from Java via JNI.
  void Cancelled(JNIEnv* env, const base::android::JavaParamRef<jobject>& obj);

  // Called from Java via JNI.
  void Closed(JNIEnv* env, const base::android::JavaParamRef<jobject>& obj);

 private:
  // The controller which owns this dialog and handles the dialog events.
  CredentialLeakPasswordChangeControllerAndroid* controller_;

  // The corresponding java object.
  base::android::ScopedJavaGlobalRef<jobject> java_object_;

  DISALLOW_COPY_AND_ASSIGN(CredentialLeakDialogPasswordChangeViewAndroid);
};

#endif  // CHROME_BROWSER_UI_ANDROID_PASSWORDS_CREDENTIAL_LEAK_DIALOG_PASSWORD_CHANGE_VIEW_ANDROID_H_
