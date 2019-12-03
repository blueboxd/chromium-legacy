// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/permission_bubble/permission_prompt_impl.h"

#include <memory>

#include "chrome/browser/content_settings/chrome_content_settings_utils.h"
#include "chrome/browser/permissions/permission_request_manager.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/ui/views/permission_bubble/permission_prompt_bubble_view.h"
#include "content/public/browser/web_contents.h"
#include "ui/views/bubble/bubble_frame_view.h"

// static
std::unique_ptr<PermissionPrompt> PermissionPrompt::Create(
    content::WebContents* web_contents,
    Delegate* delegate) {
  Browser* browser = chrome::FindBrowserWithWebContents(web_contents);
  if (!browser) {
    DLOG(WARNING) << "Permission prompt suppressed because the WebContents is "
                     "not attached to any Browser window.";
    return nullptr;
  }
  return std::make_unique<PermissionPromptImpl>(browser, web_contents,
                                                delegate);
}

PermissionPromptImpl::PermissionPromptImpl(Browser* browser,
                                           content::WebContents* web_contents,
                                           Delegate* delegate)
    : prompt_bubble_(nullptr),
      web_contents_(web_contents),
      showing_quiet_prompt_(false) {
  PermissionRequestManager* manager =
      PermissionRequestManager::FromWebContents(web_contents_);
  if (manager->ShouldCurrentRequestUseQuietUI()) {
    showing_quiet_prompt_ = true;
    // Shows the prompt as an indicator in the right side of the omnibox.
    content_settings::UpdateLocationBarUiForWebContents(web_contents_);
  } else {
    prompt_bubble_ = new PermissionPromptBubbleView(browser, delegate);
  }
}

PermissionPromptImpl::~PermissionPromptImpl() {
  if (prompt_bubble_)
    prompt_bubble_->CloseWithoutNotifyingDelegate();

  if (showing_quiet_prompt_) {
    // Hides the quiet prompt.
    content_settings::UpdateLocationBarUiForWebContents(web_contents_);
  }
}

void PermissionPromptImpl::UpdateAnchorPosition() {
  if (prompt_bubble_)
    prompt_bubble_->UpdateAnchorPosition();
}

gfx::NativeWindow PermissionPromptImpl::GetNativeWindow() {
  return prompt_bubble_ ? prompt_bubble_->GetNativeWindow() : nullptr;
}

PermissionPrompt::TabSwitchingBehavior
PermissionPromptImpl::GetTabSwitchingBehavior() {
  return PermissionPrompt::TabSwitchingBehavior::
      kDestroyPromptButKeepRequestPending;
}
