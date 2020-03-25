// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ANDROID_VR_ARCORE_DEVICE_ARCORE_CONSENT_PROMPT_H_
#define CHROME_BROWSER_ANDROID_VR_ARCORE_DEVICE_ARCORE_CONSENT_PROMPT_H_

#include "base/android/jni_android.h"
#include "base/android/jni_weak_ref.h"
#include "base/android/scoped_java_ref.h"
#include "base/callback.h"
#include "base/memory/weak_ptr.h"
#include "base/threading/thread_checker.h"
#include "chrome/browser/vr/vr_export.h"
#include "content/public/browser/xr_consent_helper.h"

namespace vr {

class VR_EXPORT ArCoreConsentPrompt : public content::XrConsentHelper {
 public:
  ArCoreConsentPrompt();
  ~ArCoreConsentPrompt() override;

  void ShowConsentPrompt(
      int render_process_id,
      int render_frame_id,
      content::XrConsentPromptLevel consent_level,
      content::OnXrUserConsentCallback response_callback) override;

  // Called from Java end.
  void OnUserConsentResult(JNIEnv* env,
                           jboolean is_granted);

 private:
  void CallDeferredUserConsentCallback(bool is_permission_granted);

  base::WeakPtr<ArCoreConsentPrompt> GetWeakPtr() {
    return weak_ptr_factory_.GetWeakPtr();
  }

  content::OnXrUserConsentCallback on_user_consent_callback_;
  content::XrConsentPromptLevel consent_level_;

  base::android::ScopedJavaGlobalRef<jobject> jdelegate_;

  base::WeakPtrFactory<ArCoreConsentPrompt> weak_ptr_factory_;
  DISALLOW_COPY_AND_ASSIGN(ArCoreConsentPrompt);
};

}  // namespace vr

#endif  // CHROME_BROWSER_ANDROID_VR_ARCORE_DEVICE_ARCORE_CONSENT_PROMPT_H_
