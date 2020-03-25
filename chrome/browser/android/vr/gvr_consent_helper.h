// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ANDROID_VR_GVR_CONSENT_HELPER_H_
#define CHROME_BROWSER_ANDROID_VR_GVR_CONSENT_HELPER_H_

#include "base/android/jni_android.h"
#include "base/callback.h"
#include "base/memory/weak_ptr.h"
#include "content/public/browser/xr_consent_helper.h"

namespace vr {
class GvrConsentHelper : public content::XrConsentHelper {
 public:
  GvrConsentHelper();

  ~GvrConsentHelper() override;

  // Caller must ensure not to call this a second time before the first dialog
  // is dismissed.
  void ShowConsentPrompt(
      int render_process_id,
      int render_frame_id,
      content::XrConsentPromptLevel consent_level,
      content::OnXrUserConsentCallback response_callback) override;
  void OnUserConsentResult(JNIEnv* env, jboolean is_granted);

 private:
  content::XrConsentPromptLevel consent_level_;

  content::OnXrUserConsentCallback on_user_consent_callback_;
  base::android::ScopedJavaGlobalRef<jobject> jdelegate_;

  base::WeakPtrFactory<GvrConsentHelper> weak_ptr_{this};

  DISALLOW_COPY_AND_ASSIGN(GvrConsentHelper);
};
}  // namespace vr

#endif  // CHROME_BROWSER_ANDROID_VR_GVR_CONSENT_HELPER_H_
