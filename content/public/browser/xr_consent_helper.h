// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_PUBLIC_BROWSER_XR_CONSENT_HELPER_H_
#define CONTENT_PUBLIC_BROWSER_XR_CONSENT_HELPER_H_

#include "base/callback_forward.h"
#include "content/common/content_export.h"
#include "content/public/browser/xr_consent_prompt_level.h"

namespace content {

using OnXrUserConsentCallback =
    base::OnceCallback<void(XrConsentPromptLevel, bool)>;

// Interface class to provide the opportunity for runtimes to ensure that any
// necessary UI is shown to request the appropriate level of user consent
// for the requested session. This is acquired via the |XrInstallHelperFactory|.
// Generally, these steps are specific per runtime, so likely this should be
// implemented for each runtime that has browser-specific installation steps.
// This should be implemented by embedders.
class CONTENT_EXPORT XrConsentHelper {
 public:
  virtual ~XrConsentHelper() = default;

  XrConsentHelper(const XrConsentHelper&) = delete;
  XrConsentHelper& operator=(const XrConsentHelper&) = delete;

  // Shows UI necessary to prompt the user for the given level of consent.
  // render_process_id and render_frame_id are passed in order to ensure that
  // any tab specific UI can be shown.
  // The callback should be guaranteed to run in the event that the object is
  // destroyed, and the object should handle being destroyed as an indication
  // that any displayed UI should be cleaned up.
  // The Callback should return either the highest level of consent that was
  // granted and true (if e.g. the user only granted consent to a lower level
  // than was initially requested), or the requested level of consent and false,
  // if the user denied consent.
  virtual void ShowConsentPrompt(int render_process_id,
                                 int render_frame_id,
                                 XrConsentPromptLevel consent_level,
                                 OnXrUserConsentCallback) = 0;

 protected:
  XrConsentHelper() = default;
};

}  // namespace content

#endif  // CONTENT_PUBLIC_BROWSER_XR_CONSENT_HELPER_H_
