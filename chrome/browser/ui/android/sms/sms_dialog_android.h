// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_ANDROID_SMS_SMS_DIALOG_ANDROID_H_
#define CHROME_BROWSER_UI_ANDROID_SMS_SMS_DIALOG_ANDROID_H_

#include "base/android/scoped_java_ref.h"
#include "content/public/browser/sms_dialog.h"

namespace url {
class Origin;
}

// The SmsDialogAndroid implements the SmsDialog interface to offer users
// the awareness and controls over receiving SMS messages.
class SmsDialogAndroid : public content::SmsDialog {
 public:
  explicit SmsDialogAndroid(const url::Origin& origin);
  ~SmsDialogAndroid() override;

  void Open(content::RenderFrameHost*, EventHandler handler) override;
  void Close() override;
  void SmsReceived() override;
  void SmsTimeout() override;

  // Report the user's action through |event_type|.
  void OnEvent(JNIEnv* env, jint event_type);

 private:
  base::android::ScopedJavaGlobalRef<jobject> java_dialog_;
  EventHandler handler_;
  DISALLOW_COPY_AND_ASSIGN(SmsDialogAndroid);
};

#endif  // CHROME_BROWSER_UI_ANDROID_SMS_SMS_DIALOG_ANDROID_H_
