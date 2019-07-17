// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_LOOKALIKES_SAFETY_TIPS_SAFETY_TIP_UI_H_
#define CHROME_BROWSER_LOOKALIKES_SAFETY_TIPS_SAFETY_TIP_UI_H_

namespace content {
class WebContents;
}

class GURL;
class Browser;

namespace safety_tips {

// This enum describes the types of warnings shown by the safety tip bubble.
enum class SafetyTipType {
  // No safety tip is/should be shown.
  kNone,

  // This URL is suspicious according to server- or client-side logic.
  kBadReputation,

  // This domain is uncommon or the site is young.
  kUncommonDomain,

  // This URL may be trying to trick users by impersonating a trustworthy URL.
  kLookalikeUrl,

  kMax = kLookalikeUrl,
};

// Shows Safety Tip UI using the specified information. |virtual_url| is the
// virtual url of the page/frame the info applies to. |safe_url| is the URL
// that the "Leave" action redirects to. Implemented in platform-specific files.
void ShowSafetyTipDialog(Browser* browser,
                         content::WebContents* web_contents,
                         SafetyTipType type,
                         const GURL& virtual_url,
                         const GURL& safe_url);

}  // namespace safety_tips

#endif  // CHROME_BROWSER_LOOKALIKES_SAFETY_TIPS_SAFETY_TIP_UI_H_
