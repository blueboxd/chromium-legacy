// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/passwords/post_save_compromised_bubble_view.h"

#include "chrome/browser/ui/passwords/passwords_model_delegate.h"

PostSaveCompromisedBubbleView::PostSaveCompromisedBubbleView(
    content::WebContents* web_contents,
    views::View* anchor_view)
    : PasswordBubbleViewBase(web_contents,
                             anchor_view,
                             /*auto_dismissable=*/false),
      controller_(PasswordsModelDelegateFromWebContents(web_contents)) {}

PostSaveCompromisedBubbleView::~PostSaveCompromisedBubbleView() = default;

PostSaveCompromisedBubbleController*
PostSaveCompromisedBubbleView::GetController() {
  return &controller_;
}

const PostSaveCompromisedBubbleController*
PostSaveCompromisedBubbleView::GetController() const {
  return &controller_;
}
