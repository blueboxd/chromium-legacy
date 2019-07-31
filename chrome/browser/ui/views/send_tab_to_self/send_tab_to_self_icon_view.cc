// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/send_tab_to_self/send_tab_to_self_icon_view.h"

#include "chrome/app/chrome_command_ids.h"
#include "chrome/app/vector_icons/vector_icons.h"
#include "chrome/browser/send_tab_to_self/send_tab_to_self_util.h"
#include "chrome/browser/ui/browser_command_controller.h"
#include "chrome/browser/ui/send_tab_to_self/send_tab_to_self_bubble_controller.h"
#include "chrome/browser/ui/views/send_tab_to_self/send_tab_to_self_bubble_view_impl.h"
#include "chrome/grit/generated_resources.h"
#include "components/omnibox/browser/omnibox_edit_model.h"
#include "components/omnibox/browser/omnibox_view.h"
#include "ui/base/l10n/l10n_util.h"

namespace send_tab_to_self {

SendTabToSelfIconView::SendTabToSelfIconView(
    CommandUpdater* command_updater,
    PageActionIconView::Delegate* delegate)
    : PageActionIconView(command_updater, IDC_SEND_TAB_TO_SELF, delegate) {
  SetVisible(false);
  SetLabel(l10n_util::GetStringUTF16(IDS_OMNIBOX_ICON_SEND_TAB_TO_SELF));
  SetUpForInOutAnimation();
}

SendTabToSelfIconView::~SendTabToSelfIconView() {}

views::BubbleDialogDelegateView* SendTabToSelfIconView::GetBubble() const {
  SendTabToSelfBubbleController* controller = GetController();
  if (!controller) {
    return nullptr;
  }

  return static_cast<SendTabToSelfBubbleViewImpl*>(
      controller->send_tab_to_self_bubble_view());
}

bool SendTabToSelfIconView::Update() {
  content::WebContents* web_contents = GetWebContents();
  if (!web_contents) {
    return false;
  }

  const bool was_visible = GetVisible();
  const OmniboxView* omnibox_view = delegate()->GetOmniboxView();
  if (!omnibox_view) {
    return false;
  }

  if (was_visible) {
    // If send tab to self icon is showing:
    // Hides the icon if the url is being editing.
    if (omnibox_view->model()->user_input_in_progress()) {
      SetVisible(false);
    }
  } else {
    // If send tab to self icon is hiding:
    // Shows the send tab to self icon in omnibox when:
    // send tab to self feature offered, and
    // mouse focuses on the omnibox, and
    // url has not been changed.
    if (send_tab_to_self::ShouldOfferFeature(web_contents) &&
        omnibox_view->model()->has_focus() &&
        !omnibox_view->model()->user_input_in_progress()) {
      // Shows the animation one time per window.
      if (!was_animation_shown_) {
        AnimateIn(IDS_OMNIBOX_ICON_SEND_TAB_TO_SELF);
        was_animation_shown_ = true;
      }
      SetVisible(true);
    }
  }

  return was_visible != GetVisible();
}

void SendTabToSelfIconView::OnExecuting(
    PageActionIconView::ExecuteSource execute_source) {}

const gfx::VectorIcon& SendTabToSelfIconView::GetVectorIcon() const {
  return kSendTabToSelfIcon;
}

base::string16 SendTabToSelfIconView::GetTextForTooltipAndAccessibleName()
    const {
  return l10n_util::GetStringUTF16(IDS_OMNIBOX_TOOLTIP_SEND_TAB_TO_SELF);
}

SendTabToSelfBubbleController* SendTabToSelfIconView::GetController() const {
  content::WebContents* web_contents = GetWebContents();
  if (!web_contents) {
    return nullptr;
  }
  return SendTabToSelfBubbleController::CreateOrGetFromWebContents(
      web_contents);
}

void SendTabToSelfIconView::AnimationEnded(const gfx::Animation* animation) {
  IconLabelBubbleView::AnimationEnded(animation);
}

}  // namespace send_tab_to_self
