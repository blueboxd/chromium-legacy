// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_VR_CONSENT_WIN_XR_CONSENT_HELPER_H_
#define CHROME_BROWSER_VR_CONSENT_WIN_XR_CONSENT_HELPER_H_

#include "content/public/browser/xr_consent_helper.h"

namespace vr {

// Windows makes use of a factory singleton to manage its consent prompts. Once
// a prompt has been created, it listens directly to page lifecycle events to
// determine when to tear itself down. This class is a wrapper around that
// factory so that the pattern can be integrated with the |XrIntegrationClient|.
// TODO(crbug.com/1064384): Remove this class and the factory when possible.
class WinXrConsentHelper : public content::XrConsentHelper {
 public:
  WinXrConsentHelper() = default;
  ~WinXrConsentHelper() override = default;
  WinXrConsentHelper(const WinXrConsentHelper&) = delete;
  WinXrConsentHelper& operator=(const WinXrConsentHelper&) = delete;
  void ShowConsentPrompt(
      int render_process_id,
      int render_frame_id,
      content::XrConsentPromptLevel consent_level,
      content::OnXrUserConsentCallback response_callback) override;
};

}  // namespace vr

#endif  // CHROME_BROWSER_VR_CONSENT_WIN_XR_CONSENT_HELPER_H_
