// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_PERMISSION_BUBBLE_PERMISSION_PROMPT_IMPL_H_
#define CHROME_BROWSER_UI_VIEWS_PERMISSION_BUBBLE_PERMISSION_PROMPT_IMPL_H_

#include "base/macros.h"
#include "components/permissions/permission_prompt.h"

class Browser;
class PermissionPromptBubbleView;

namespace content {
class WebContents;
}  // namespace content

// This object will create or trigger UI to reflect that a website is requesting
// a permission. The UI is usually a popup bubble, but may instead be a location
// bar icon (the "quiet" prompt).
class PermissionPromptImpl : public permissions::PermissionPrompt {
 public:
  PermissionPromptImpl(Browser* browser,
                       content::WebContents* web_contents,
                       Delegate* delegate);
  ~PermissionPromptImpl() override;

  // permissions::PermissionPrompt:
  void UpdateAnchorPosition() override;
  TabSwitchingBehavior GetTabSwitchingBehavior() override;

  PermissionPromptBubbleView* prompt_bubble_for_testing() {
    return prompt_bubble_;
  }

 private:
  // The popup bubble. Not owned by this class; it will delete itself when a
  // decision is made.
  PermissionPromptBubbleView* prompt_bubble_;

  // The web contents whose location bar should show the quiet prompt.
  content::WebContents* web_contents_;

  bool showing_quiet_prompt_;

  DISALLOW_COPY_AND_ASSIGN(PermissionPromptImpl);
};

#endif  // CHROME_BROWSER_UI_VIEWS_PERMISSION_BUBBLE_PERMISSION_PROMPT_IMPL_H_
