// Copyright (c) 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/autofill/autofill_bubble_handler_impl.h"

#include "chrome/browser/autofill/personal_data_manager_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/autofill/payments/local_card_migration_bubble.h"
#include "chrome/browser/ui/autofill/payments/save_card_bubble_view.h"
#include "chrome/browser/ui/autofill/payments/save_card_ui.h"
#include "chrome/browser/ui/views/autofill/payments/local_card_migration_bubble_views.h"
#include "chrome/browser/ui/views/autofill/payments/local_card_migration_icon_view.h"
#include "chrome/browser/ui/views/autofill/payments/save_card_bubble_views.h"
#include "chrome/browser/ui/views/autofill/payments/save_card_failure_bubble_views.h"
#include "chrome/browser/ui/views/autofill/payments/save_card_icon_view.h"
#include "chrome/browser/ui/views/autofill/payments/save_card_manage_cards_bubble_views.h"
#include "chrome/browser/ui/views/autofill/payments/save_card_offer_bubble_views.h"
#include "chrome/browser/ui/views/autofill/payments/save_card_sign_in_promo_bubble_views.h"
#include "chrome/browser/ui/views/frame/toolbar_button_provider.h"
#include "chrome/browser/ui/views/page_action/page_action_icon_view.h"
#include "chrome/browser/ui/views/profiles/avatar_toolbar_button.h"
#include "components/autofill/core/browser/personal_data_manager.h"
#include "components/autofill/core/browser/personal_data_manager_observer.h"
#include "ui/views/bubble/bubble_dialog_delegate_view.h"

namespace autofill {

AutofillBubbleHandlerImpl::AutofillBubbleHandlerImpl(
    ToolbarButtonProvider* toolbar_button_provider,
    Profile* profile)
    : toolbar_button_provider_(toolbar_button_provider) {
  if (profile) {
    personal_data_manager_observer_.Add(
        PersonalDataManagerFactory::GetForProfile(
            profile->GetOriginalProfile()));
  }
}

AutofillBubbleHandlerImpl::~AutofillBubbleHandlerImpl() = default;

// TODO(crbug.com/932818): Clean up this two functions and add helper for shared
// code.
SaveCardBubbleView* AutofillBubbleHandlerImpl::ShowSaveCreditCardBubble(
    content::WebContents* web_contents,
    SaveCardBubbleController* controller,
    bool is_user_gesture) {
  autofill::BubbleType bubble_type = controller->GetBubbleType();
  PageActionIconView* icon_view =
      toolbar_button_provider_->GetPageActionIconView(
          PageActionIconType::kSaveCard);
  views::View* anchor_view =
      toolbar_button_provider_->GetAnchorView(PageActionIconType::kSaveCard);

  autofill::SaveCardBubbleViews* bubble = nullptr;
  switch (bubble_type) {
    case autofill::BubbleType::LOCAL_SAVE:
    case autofill::BubbleType::UPLOAD_SAVE:
      bubble = new autofill::SaveCardOfferBubbleViews(anchor_view, web_contents,
                                                      controller);
      break;
    case autofill::BubbleType::SIGN_IN_PROMO:
      bubble = new autofill::SaveCardSignInPromoBubbleViews(
          anchor_view, web_contents, controller);
      break;
    case autofill::BubbleType::MANAGE_CARDS:
      bubble = new autofill::SaveCardManageCardsBubbleViews(
          anchor_view, web_contents, controller);
      break;
    case autofill::BubbleType::FAILURE:
      bubble = new autofill::SaveCardFailureBubbleViews(
          anchor_view, web_contents, controller);
      break;
    case autofill::BubbleType::UPLOAD_IN_PROGRESS:
    case autofill::BubbleType::INACTIVE:
      break;
  }
  DCHECK(bubble);

  if (icon_view)
    bubble->SetHighlightedButton(icon_view);

  views::BubbleDialogDelegateView::CreateBubble(bubble);
  bubble->Show(is_user_gesture ? autofill::SaveCardBubbleViews::USER_GESTURE
                               : autofill::SaveCardBubbleViews::AUTOMATIC);
  return bubble;
}

LocalCardMigrationBubble*
AutofillBubbleHandlerImpl::ShowLocalCardMigrationBubble(
    content::WebContents* web_contents,
    LocalCardMigrationBubbleController* controller,
    bool is_user_gesture) {
  autofill::LocalCardMigrationBubbleViews* bubble =
      new autofill::LocalCardMigrationBubbleViews(
          toolbar_button_provider_->GetAnchorView(
              PageActionIconType::kLocalCardMigration),
          web_contents, controller);

  PageActionIconView* icon_view =
      toolbar_button_provider_->GetPageActionIconView(
          PageActionIconType::kLocalCardMigration);
  if (icon_view)
    bubble->SetHighlightedButton(icon_view);

  views::BubbleDialogDelegateView::CreateBubble(bubble);
  bubble->Show(is_user_gesture
                   ? autofill::LocalCardMigrationBubbleViews::USER_GESTURE
                   : autofill::LocalCardMigrationBubbleViews::AUTOMATIC);
  return bubble;
}

void AutofillBubbleHandlerImpl::OnPasswordSaved() {
  ShowAvatarHighlightAnimation();
}

void AutofillBubbleHandlerImpl::OnCreditCardSaved() {
  ShowAvatarHighlightAnimation();
}

void AutofillBubbleHandlerImpl::ShowAvatarHighlightAnimation() {
  AvatarToolbarButton* avatar =
      toolbar_button_provider_->GetAvatarToolbarButton();
  if (avatar)
    avatar->ShowAvatarHighlightAnimation();
}

}  // namespace autofill
