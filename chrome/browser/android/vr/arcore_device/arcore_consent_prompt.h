// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ANDROID_VR_ARCORE_DEVICE_ARCORE_CONSENT_PROMPT_H_
#define CHROME_BROWSER_ANDROID_VR_ARCORE_DEVICE_ARCORE_CONSENT_PROMPT_H_

#include "base/android/jni_android.h"
#include "base/android/jni_weak_ref.h"
#include "base/callback.h"
#include "chrome/browser/vr/service/arcore_consent_prompt_interface.h"
#include "chrome/browser/vr/vr_export.h"

namespace vr {

class VR_EXPORT ArcoreConsentPrompt : public ArcoreConsentPromptInterface {
 public:
  void ShowConsentPrompt(
      int render_process_id,
      int render_frame_id,
      base::OnceCallback<void(bool)> response_callback) override;

  ArcoreConsentPrompt();
  ~ArcoreConsentPrompt();

  // device::VrDevicePermissionProvider:
  void GetUserPermission(int render_process_id,
                         int render_frame_id,
                         base::OnceCallback<void(bool)> response_callback);

  void OnUserConsentResult(JNIEnv* env,
                           const base::android::JavaParamRef<jobject>& j_caller,
                           jboolean is_granted);

 private:
  base::OnceCallback<void(bool)> on_user_consent_callback_;
};

}  // namespace vr

#endif  // CHROME_BROWSER_ANDROID_VR_ARCORE_DEVICE_ARCORE_CONSENT_PROMPT_H_
