// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_PERMISSION_BUBBLE_PERMISSION_PROMPT_BUBBLE_VIEW_H_
#define CHROME_BROWSER_UI_VIEWS_PERMISSION_BUBBLE_PERMISSION_PROMPT_BUBBLE_VIEW_H_

#include "base/macros.h"
#include "chrome/browser/ui/permission_bubble/permission_prompt.h"
#include "ui/views/bubble/bubble_dialog_delegate_view.h"

class Browser;

// Bubble that prompts the user to grant or deny a permission request from a
// website.
class PermissionPromptBubbleView : public views::BubbleDialogDelegateView {
 public:
  PermissionPromptBubbleView(Browser* browser,
                             PermissionPrompt::Delegate* delegate);

  // Anchors the bubble to the view or rectangle returned from
  // bubble_anchor_util::GetPageInfoAnchorConfiguration.
  void UpdateAnchorPosition();

  // Closes the bubble without notifying |delegate_|. Called when the
  // controlling PermissionPrompt is removed from the permission system, and so
  // the delegate will have lost the relevant reference. This can happen when
  // the user changes tabs or initiates a navigation without interacting with
  // the UI.
  void CloseWithoutNotifyingDelegate();

  // Returns the gfx::NativeWindow for the bubble's widget.
  gfx::NativeWindow GetNativeWindow();

  // views::BubbleDialogDelegateView:
  void AddedToWidget() override;
  bool ShouldShowCloseButton() const override;
  base::string16 GetWindowTitle() const override;
  bool Cancel() override;
  bool Accept() override;
  bool Close() override;

 private:
  void AddPermissionRequestLine(PermissionRequest* request);

  void Show();

  Browser* const browser_;
  PermissionPrompt::Delegate* const delegate_;

  // The requesting domain's name or origin.
  const PermissionPrompt::DisplayNameOrOrigin name_or_origin_;

  // Whether to notify |delegate_| of a decision. Set to false when
  // CloseWithNotifyingDelegate is called; see the comment on that method.
  bool notify_delegate_ = true;

  DISALLOW_COPY_AND_ASSIGN(PermissionPromptBubbleView);
};

#endif  // CHROME_BROWSER_UI_VIEWS_PERMISSION_BUBBLE_PERMISSION_PROMPT_BUBBLE_VIEW_H_
