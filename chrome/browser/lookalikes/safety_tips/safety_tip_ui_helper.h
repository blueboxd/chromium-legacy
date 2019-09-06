// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_LOOKALIKES_SAFETY_TIPS_SAFETY_TIP_UI_HELPER_H_
#define CHROME_BROWSER_LOOKALIKES_SAFETY_TIPS_SAFETY_TIP_UI_HELPER_H_

#include "chrome/browser/lookalikes/safety_tips/safety_tip_ui.h"
#include "components/security_state/core/security_state.h"

namespace content {
class WebContents;
}

namespace safety_tips {

// Records a histogram for a user's interaction with a Safety Tip in the given
// |web_contents|.
void RecordSafetyTipInteractionHistogram(content::WebContents* web_contents,
                                         SafetyTipInteraction interaction);

// Invoke action when 'leave site' button is clicked, and records a histogram.
// Navigates to a safe URL, replacing the current page in the process.
void LeaveSite(content::WebContents* web_contents);

// Get the title and description string IDs needed to describe the applicable
// warning type.  Handles both Android and desktop warnings.
int GetSafetyTipTitleId(security_state::SafetyTipStatus warning_type);
int GetSafetyTipDescriptionId(security_state::SafetyTipStatus warning_type);

}  // namespace safety_tips

#endif  // CHROME_BROWSER_LOOKALIKES_SAFETY_TIPS_SAFETY_TIP_UI_HELPER_H_
