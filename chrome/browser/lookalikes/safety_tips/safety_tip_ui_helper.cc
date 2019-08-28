// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/lookalikes/safety_tips/safety_tip_ui_helper.h"

#include "build/build_config.h"
#include "components/strings/grit/components_strings.h"
#include "url/gurl.h"

namespace {

// URL that the "leave site" button aborts to.
const char kSafeUrl[] = "chrome://newtab";

}  // namespace

namespace safety_tips {

void LeaveSite(content::WebContents* web_contents) {
  content::OpenURLParams params(
      GURL(kSafeUrl), content::Referrer(), WindowOpenDisposition::CURRENT_TAB,
      ui::PAGE_TRANSITION_AUTO_TOPLEVEL, false /* is_renderer_initiated */);
  params.should_replace_current_entry = true;
  web_contents->OpenURL(params);
}

int GetSafetyTipTitleId(SafetyTipType warning_type) {
  switch (warning_type) {
    case SafetyTipType::kBadReputation:
    case SafetyTipType::kLookalikeUrl:
#if defined(OS_ANDROID)
      return IDS_SAFETY_TIP_ANDROID_BAD_REPUTATION_TITLE;
#else
      return IDS_PAGE_INFO_SAFETY_TIP_BAD_REPUTATION_TITLE;
#endif
    // These don't have strings yet, so they're just an error:
    case SafetyTipType::kUncommonDomain:
    case SafetyTipType::kNone:
      NOTREACHED();
  }
  NOTREACHED();
  return 0;
}

int GetSafetyTipDescriptionId(SafetyTipType warning_type) {
  switch (warning_type) {
    case SafetyTipType::kBadReputation:
#if defined(OS_ANDROID)
      return IDS_SAFETY_TIP_ANDROID_BAD_REPUTATION_DESCRIPTION;
#else
      return IDS_PAGE_INFO_SAFETY_TIP_BAD_REPUTATION_DESCRIPTION;
#endif
    case SafetyTipType::kLookalikeUrl:
      return IDS_LOOKALIKE_URL_PRIMARY_PARAGRAPH;
    // These don't have strings yet, so they're just an error:
    case SafetyTipType::kUncommonDomain:
    case SafetyTipType::kNone:
      NOTREACHED();
  }
  NOTREACHED();
  return 0;
}

}  // namespace safety_tips
