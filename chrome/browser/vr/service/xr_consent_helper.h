// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_VR_SERVICE_XR_CONSENT_HELPER_H_
#define CHROME_BROWSER_VR_SERVICE_XR_CONSENT_HELPER_H_

#include "base/callback_forward.h"
#include "chrome/browser/vr/service/xr_consent_prompt_level.h"

namespace vr {

typedef base::OnceCallback<void(XrConsentPromptLevel, bool)>
    OnUserConsentCallback;

class XrConsentHelper {
 public:
  virtual ~XrConsentHelper() = default;

  XrConsentHelper(const XrConsentHelper&) = delete;
  XrConsentHelper& operator=(const XrConsentHelper&) = delete;

  virtual void ShowConsentPrompt(int render_process_id,
                                 int render_frame_id,
                                 XrConsentPromptLevel consent_level,
                                 OnUserConsentCallback) = 0;

 protected:
  XrConsentHelper() = default;
};

}  // namespace vr

#endif  // CHROME_BROWSER_VR_SERVICE_XR_CONSENT_HELPER_H_
