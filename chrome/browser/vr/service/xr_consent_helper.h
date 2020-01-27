// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_VR_SERVICE_XR_CONSENT_HELPER_H_
#define CHROME_BROWSER_VR_SERVICE_XR_CONSENT_HELPER_H_

#include <memory>

#include "base/callback_forward.h"
#include "base/macros.h"
#include "build/build_config.h"
#include "chrome/browser/vr/service/xr_consent_prompt_level.h"

#if defined(OS_ANDROID)
#include "base/android/jni_android.h"
#endif

namespace content {
class WebContents;
}

namespace vr {

typedef base::OnceCallback<void(XrConsentPromptLevel, bool)>
    OnUserConsentCallback;

class XrConsentHelper {
 public:
  virtual ~XrConsentHelper();

  virtual void ShowConsentPrompt(int render_process_id,
                                 int render_frame_id,
                                 XrConsentPromptLevel consent_level,
                                 OnUserConsentCallback) = 0;

 protected:
  XrConsentHelper();

  // Gets a WebContents from a given |render_process_id| and |render_frame_id|.
  // The return value is guaranteed non-null.
  static content::WebContents* GetWebContentsFromRenderer(int render_process_id,
                                                          int render_frame_id);

#if defined(OS_ANDROID)
  static base::android::ScopedJavaLocalRef<jobject> GetTabFromRenderer(
      int render_process_id,
      int render_frame_id);
#endif

 private:
  XrConsentHelper(const XrConsentHelper&) = delete;
  XrConsentHelper& operator=(const XrConsentHelper&) = delete;
};

}  // namespace vr

#endif  // CHROME_BROWSER_VR_SERVICE_XR_CONSENT_HELPER_H_
