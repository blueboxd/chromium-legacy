// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_LOOKALIKES_SAFETY_TIPS_SAFETY_TIP_UI_H_
#define CHROME_BROWSER_LOOKALIKES_SAFETY_TIPS_SAFETY_TIP_UI_H_

#include "build/build_config.h"
#include "components/security_state/core/security_state.h"

namespace content {
class WebContents;
}

class GURL;

namespace safety_tips {

// Shows Safety Tip UI using the specified information if it is not already
// showing. |virtual_url| is the virtual url of the page/frame the info applies
// to. |safe_url| is the URL that the "Leave" action redirects to. Implemented
// in platform-specific files.
void ShowSafetyTipDialog(content::WebContents* web_contents,
                         security_state::SafetyTipStatus type,
                         const GURL& virtual_url);

}  // namespace safety_tips

#endif  // CHROME_BROWSER_LOOKALIKES_SAFETY_TIPS_SAFETY_TIP_UI_H_
