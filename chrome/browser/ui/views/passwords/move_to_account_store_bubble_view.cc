// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/passwords/move_to_account_store_bubble_view.h"

#include "base/bind.h"
#include "chrome/browser/ui/passwords/bubble_controllers/move_to_account_store_bubble_controller.h"
#include "chrome/browser/ui/passwords/passwords_model_delegate.h"

MoveToAccountStoreBubbleView::MoveToAccountStoreBubbleView(
    content::WebContents* web_contents,
    views::View* anchor_view)
    : PasswordBubbleViewBase(web_contents,
                             anchor_view,
                             /*auto_dismissable=*/false),
      controller_(PasswordsModelDelegateFromWebContents(web_contents)) {
  SetAcceptCallback(
      base::BindOnce(&MoveToAccountStoreBubbleController::AcceptMove,
                     base::Unretained(&controller_)));
}

MoveToAccountStoreBubbleView::~MoveToAccountStoreBubbleView() = default;

MoveToAccountStoreBubbleController*
MoveToAccountStoreBubbleView::GetController() {
  return &controller_;
}

const MoveToAccountStoreBubbleController*
MoveToAccountStoreBubbleView::GetController() const {
  return &controller_;
}
