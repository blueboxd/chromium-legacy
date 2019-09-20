// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SHARING_SHARING_SERVICE_PROXY_ANDROID_H_
#define CHROME_BROWSER_SHARING_SHARING_SERVICE_PROXY_ANDROID_H_

#include "base/android/jni_android.h"
#include "base/macros.h"
#include "chrome/browser/sharing/click_to_call/click_to_call_message_handler_android.h"
#include "chrome/browser/sharing/sharing_message_handler.h"

class SharingService;

// Allows Android to query the Sharing Service for information.
class SharingServiceProxyAndroid {
 public:
  explicit SharingServiceProxyAndroid(SharingService* sharing_service);
  ~SharingServiceProxyAndroid();

  ClickToCallMessageHandler* click_to_call_message_handler() {
    return &click_to_call_message_handler_;
  }

  void SendSharedClipboardMessage(
      JNIEnv* env,
      const base::android::JavaParamRef<jstring>& j_guid,
      const base::android::JavaParamRef<jstring>& j_text,
      const base::android::JavaParamRef<jobject>& j_runnable);

  void GetDeviceCandidates(
      JNIEnv* env,
      const base::android::JavaParamRef<jobject>& j_device_info,
      jint j_required_feature);

  void AddDeviceCandidatesInitializedObserver(
      JNIEnv* env,
      const base::android::JavaParamRef<jobject>& j_runnable);

 private:
  SharingService* sharing_service_ = nullptr;
  ClickToCallMessageHandler click_to_call_message_handler_;

  DISALLOW_COPY_AND_ASSIGN(SharingServiceProxyAndroid);
};

#endif  // CHROME_BROWSER_SHARING_SHARING_SERVICE_PROXY_ANDROID_H_
