// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/sharing/click_to_call/click_to_call_icon_view.h"

#include <algorithm>

#include "base/memory/ptr_util.h"
#include "chrome/browser/sharing/click_to_call/click_to_call_ui_controller.h"
#include "chrome/browser/ui/views/sharing/click_to_call/click_to_call_dialog_view.h"
#include "components/vector_icons/vector_icons.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/material_design/material_design_controller.h"
#include "ui/gfx/paint_vector_icon.h"
#include "ui/strings/grit/ui_strings.h"
#include "ui/views/animation/ink_drop.h"

namespace {
ClickToCallUiController* GetControllerFromWebContents(
    content::WebContents* web_contents) {
  if (!web_contents)
    return nullptr;

  return ClickToCallUiController::GetOrCreateFromWebContents(web_contents);
}
}  // namespace

ClickToCallIconView::ClickToCallIconView(PageActionIconView::Delegate* delegate)
    : SharingIconView(delegate) {}

ClickToCallIconView::~ClickToCallIconView() = default;

views::BubbleDialogDelegateView* ClickToCallIconView::GetBubble() const {
  auto* controller = GetControllerFromWebContents(GetWebContents());
  return controller ? static_cast<ClickToCallDialogView*>(controller->dialog())
                    : nullptr;
}

bool ClickToCallIconView::Update() {
  auto* controller = GetControllerFromWebContents(GetWebContents());
  if (!controller)
    return false;

  if (should_show_error() != controller->send_failed()) {
    set_should_show_error(controller->send_failed());
    UpdateIconImage();
  }

  if (controller->is_loading())
    StartLoadingAnimation();
  else
    StopLoadingAnimation(IDS_BROWSER_SHARING_CLICK_TO_CALL_DIALOG_SEND_SUCCESS);

  const bool is_bubble_showing = IsBubbleShowing();

  if (is_bubble_showing || isLoadingAnimationVisible() ||
      last_controller_ != controller)
    ResetSlideAnimation(/*show=*/false);

  last_controller_ = controller;

  const bool is_visible =
      is_bubble_showing || isLoadingAnimationVisible() || label()->GetVisible();
  const bool visibility_changed = GetVisible() != is_visible;

  SetVisible(is_visible);
  UpdateInkDrop(is_bubble_showing);
  return visibility_changed;
}

const gfx::VectorIcon& ClickToCallIconView::GetVectorIcon() const {
  return vector_icons::kCallIcon;
}

base::string16 ClickToCallIconView::GetTextForTooltipAndAccessibleName() const {
  return l10n_util::GetStringUTF16(
      IDS_BROWSER_SHARING_CLICK_TO_CALL_DIALOG_TITLE_LABEL);
}
