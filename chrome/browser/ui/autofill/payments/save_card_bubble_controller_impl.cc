// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/autofill/payments/save_card_bubble_controller_impl.h"

#include <stddef.h>
#include <utility>

#include "base/metrics/field_trial_params.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/autofill/personal_data_manager_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/signin/account_consistency_mode_manager.h"
#include "chrome/browser/signin/identity_manager_factory.h"
#include "chrome/browser/signin/signin_ui_util.h"
#include "chrome/browser/ssl/security_state_tab_helper.h"
#include "chrome/browser/sync/profile_sync_service_factory.h"
#include "chrome/browser/ui/autofill/payments/autofill_ui_util.h"
#include "chrome/browser/ui/autofill/payments/payments_ui_constants.h"
#include "chrome/browser/ui/autofill/payments/save_card_bubble_view.h"
#include "chrome/browser/ui/autofill/payments/save_card_ui.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/chrome_pages.h"
#include "chrome/browser/ui/location_bar/location_bar.h"
#include "chrome/browser/ui/page_action/page_action_icon_container.h"
#include "chrome/browser/ui/sync/sync_promo_ui.h"
#include "chrome/common/url_constants.h"
#include "components/autofill/core/browser/autofill_experiments.h"
#include "components/autofill/core/browser/autofill_metrics.h"
#include "components/autofill/core/browser/personal_data_manager.h"
#include "components/autofill/core/common/autofill_clock.h"
#include "components/autofill/core/common/autofill_constants.h"
#include "components/autofill/core/common/autofill_features.h"
#include "components/autofill/core/common/autofill_payments_features.h"
#include "components/autofill/core/common/autofill_prefs.h"
#include "components/prefs/pref_service.h"
#include "components/signin/public/base/signin_buildflags.h"
#include "components/signin/public/identity_manager/identity_manager.h"
#include "components/strings/grit/components_strings.h"
#include "components/sync/driver/sync_service.h"
#include "components/user_prefs/user_prefs.h"
#include "content/public/browser/navigation_handle.h"
#include "ui/base/l10n/l10n_util.h"

namespace autofill {

SaveCardBubbleControllerImpl::SaveCardBubbleControllerImpl(
    content::WebContents* web_contents)
    : content::WebContentsObserver(web_contents),
      web_contents_(web_contents),
      pref_service_(
          user_prefs::UserPrefs::Get(web_contents->GetBrowserContext())) {
  security_level_ =
      SecurityStateTabHelper::FromWebContents(web_contents)->GetSecurityLevel();

  personal_data_manager_ =
      PersonalDataManagerFactory::GetInstance()->GetForProfile(
          Profile::FromBrowserContext(web_contents->GetBrowserContext()));
}

SaveCardBubbleControllerImpl::~SaveCardBubbleControllerImpl() {
  if (save_card_bubble_view_)
    save_card_bubble_view_->Hide();
}

void SaveCardBubbleControllerImpl::OfferLocalSave(
    const CreditCard& card,
    AutofillClient::SaveCreditCardOptions options,
    AutofillClient::LocalSaveCardPromptCallback save_card_prompt_callback) {
  // Don't show the bubble if it's already visible.
  if (save_card_bubble_view_)
    return;

  is_upload_save_ = false;
  is_reshow_ = false;
  options_ = options;
  legal_message_lines_.clear();

  card_ = card;
  local_save_card_prompt_callback_ = std::move(save_card_prompt_callback);
  current_bubble_type_ = BubbleType::LOCAL_SAVE;
  if (options.show_prompt) {
    ShowBubble();
    AutofillMetrics::LogSaveCardPromptMetric(
        AutofillMetrics::SAVE_CARD_PROMPT_SHOW_REQUESTED, is_upload_save_,
        is_reshow_, options_,
        pref_service_->GetInteger(
            prefs::kAutofillAcceptSaveCreditCardPromptState),
        GetSecurityLevel(), GetSyncState());
  } else {
    ShowIconOnly();
  }
}

void SaveCardBubbleControllerImpl::OfferUploadSave(
    const CreditCard& card,
    std::unique_ptr<base::DictionaryValue> legal_message,
    AutofillClient::SaveCreditCardOptions options,
    AutofillClient::UploadSaveCardPromptCallback save_card_prompt_callback) {
  // Don't show the bubble if it's already visible.
  if (save_card_bubble_view_)
    return;

  // Fetch the logged-in user's AccountInfo if it has not yet been done.
  if (options.should_request_name_from_user && account_info_.IsEmpty())
    FetchAccountInfo();

  is_upload_save_ = true;
  is_reshow_ = false;
  options_ = options;
  if (options.show_prompt) {
    // Can't move this into the other "if (show_bubble_)" below because an
    // invalid legal message would skip it.
    AutofillMetrics::LogSaveCardPromptMetric(
        AutofillMetrics::SAVE_CARD_PROMPT_SHOW_REQUESTED, is_upload_save_,
        is_reshow_, options_,
        pref_service_->GetInteger(
            prefs::kAutofillAcceptSaveCreditCardPromptState),
        GetSecurityLevel(), GetSyncState());
  }

  if (!LegalMessageLine::Parse(*legal_message, &legal_message_lines_,
                               /*escape_apostrophes=*/true)) {
    AutofillMetrics::LogSaveCardPromptMetric(
        AutofillMetrics::SAVE_CARD_PROMPT_END_INVALID_LEGAL_MESSAGE,
        is_upload_save_, is_reshow_, options_,
        pref_service_->GetInteger(
            prefs::kAutofillAcceptSaveCreditCardPromptState),
        GetSecurityLevel(), GetSyncState());
    return;
  }

  card_ = card;
  upload_save_card_prompt_callback_ = std::move(save_card_prompt_callback);
  current_bubble_type_ = BubbleType::UPLOAD_SAVE;

  if (options_.show_prompt)
    ShowBubble();
  else
    ShowIconOnly();
}

void SaveCardBubbleControllerImpl::ShowBubbleForSignInPromo() {
  if (!ShouldShowSignInPromo())
    return;

  current_bubble_type_ = BubbleType::SIGN_IN_PROMO;

  // If DICe is disabled, then we need to know whether the user is signed in
  // to determine whether or not to show a sign-in vs sync promo.
  if (GetAccountInfo().IsEmpty())
    FetchAccountInfo();
  ShowBubble();
}

// Exists for testing purposes only.
void SaveCardBubbleControllerImpl::ShowBubbleForManageCardsForTesting(
    const CreditCard& card) {
  card_ = card;
  current_bubble_type_ = BubbleType::MANAGE_CARDS;
  ShowBubble();
}

void SaveCardBubbleControllerImpl::UpdateIconForSaveCardSuccess() {
  // TODO(crbug.com/964127): Dismisses the icon and triggers a highlight
  // animation of the avatar button.
}

void SaveCardBubbleControllerImpl::UpdateIconForSaveCardFailure() {
  current_bubble_type_ = BubbleType::FAILURE;
  ShowIconOnly();
}

void SaveCardBubbleControllerImpl::ShowBubbleForSaveCardFailureForTesting() {
  current_bubble_type_ = BubbleType::FAILURE;
  ShowBubble();
}

void SaveCardBubbleControllerImpl::HideBubble() {
  if (save_card_bubble_view_) {
    save_card_bubble_view_->Hide();
    save_card_bubble_view_ = nullptr;
  }
}

void SaveCardBubbleControllerImpl::ReshowBubble() {
  // Don't show the bubble if it's already visible.
  if (save_card_bubble_view_)
    return;

  is_reshow_ = true;

  if (current_bubble_type_ == BubbleType::LOCAL_SAVE ||
      current_bubble_type_ == BubbleType::UPLOAD_SAVE) {
    AutofillMetrics::LogSaveCardPromptMetric(
        AutofillMetrics::SAVE_CARD_PROMPT_SHOW_REQUESTED, is_upload_save_,
        is_reshow_, options_,
        pref_service_->GetInteger(
            prefs::kAutofillAcceptSaveCreditCardPromptState),
        GetSecurityLevel(), GetSyncState());
  }

  ShowBubble();
}

bool SaveCardBubbleControllerImpl::IsIconVisible() const {
  // If there is no bubble to show, then there should be no icon.
  return current_bubble_type_ != BubbleType::INACTIVE;
}

SaveCardBubbleView* SaveCardBubbleControllerImpl::save_card_bubble_view()
    const {
  return save_card_bubble_view_;
}

base::string16 SaveCardBubbleControllerImpl::GetWindowTitle() const {
  switch (current_bubble_type_) {
    case BubbleType::LOCAL_SAVE:
      return l10n_util::GetStringUTF16(
          IDS_AUTOFILL_SAVE_CARD_PROMPT_TITLE_LOCAL);
    case BubbleType::UPLOAD_SAVE: {
      if (features::ShouldShowImprovedUserConsentForCreditCardSave()) {
        // No thanks button is fully enabled on Windows, MacOS and Linux. Show
        // window title based on |kAutofillSaveCreditCardUsesImprovedMessaging|
        // experiment.
        std::string param = base::GetFieldTrialParamValueByFeature(
            features::kAutofillSaveCreditCardUsesImprovedMessaging,
            features::kAutofillSaveCreditCardUsesImprovedMessagingParamName);
        if (param ==
            features::
                kAutofillSaveCreditCardUsesImprovedMessagingParamValueStoreCard) {
          return l10n_util::GetStringUTF16(
              IDS_AUTOFILL_SAVE_CARD_PROMPT_TITLE_TO_CLOUD_COPY_TEST_STORE_CARD);
        }
        if (param ==
            features::
                kAutofillSaveCreditCardUsesImprovedMessagingParamValueStoreBillingDetails) {
          return l10n_util::GetStringUTF16(
              IDS_AUTOFILL_SAVE_CARD_PROMPT_TITLE_TO_CLOUD_COPY_TEST_STORE_BILLING_DETAILS);
        }
        if (param ==
            features::
                kAutofillSaveCreditCardUsesImprovedMessagingParamValueAddCard) {
          return l10n_util::GetStringUTF16(
              IDS_AUTOFILL_SAVE_CARD_PROMPT_TITLE_TO_CLOUD_COPY_TEST_ADD_CARD);
        }
        if (param ==
            features::
                kAutofillSaveCreditCardUsesImprovedMessagingParamValueConfirmAndSaveCard) {
          return l10n_util::GetStringUTF16(
              IDS_AUTOFILL_SAVE_CARD_PROMPT_TITLE_TO_CLOUD_COPY_TEST_CONFIRM_AND_SAVE_CARD);
        }
        // Control group.
        return l10n_util::GetStringUTF16(
            IDS_AUTOFILL_SAVE_CARD_PROMPT_TITLE_TO_CLOUD_V4);
      }
      return l10n_util::GetStringUTF16(
          IDS_AUTOFILL_SAVE_CARD_PROMPT_TITLE_TO_CLOUD_V3);
    }
    case BubbleType::SIGN_IN_PROMO:
#if BUILDFLAG(ENABLE_DICE_SUPPORT)
      if (AccountConsistencyModeManager::IsDiceEnabledForProfile(
              GetProfile())) {
        return l10n_util::GetStringUTF16(IDS_AUTOFILL_SYNC_PROMO_MESSAGE);
      }
#endif
      return l10n_util::GetStringUTF16(IDS_AUTOFILL_CARD_SAVED);
    case BubbleType::MANAGE_CARDS:
      return l10n_util::GetStringUTF16(IDS_AUTOFILL_CARD_SAVED);
    case BubbleType::FAILURE:
      return l10n_util::GetStringUTF16(IDS_AUTOFILL_FAILURE_BUBBLE_TITLE);
    case BubbleType::INACTIVE:
      NOTREACHED();
      return base::string16();
  }
}

base::string16 SaveCardBubbleControllerImpl::GetExplanatoryMessage() const {
  if (current_bubble_type_ == BubbleType::FAILURE)
    return l10n_util::GetStringUTF16(IDS_AUTOFILL_FAILURE_BUBBLE_EXPLANATION);

  if (current_bubble_type_ != BubbleType::UPLOAD_SAVE)
    return base::string16();

  bool offer_to_save_on_device_message =
      OfferStoreUnmaskedCards(
          web_contents_->GetBrowserContext()->IsOffTheRecord()) &&
      !IsAutofillNoLocalSaveOnUploadSuccessExperimentEnabled();
  // TODO(crbug.com/961082): Might need to revisit strings for name fix flow.
  if (options_.should_request_name_from_user) {
    return l10n_util::GetStringUTF16(
        offer_to_save_on_device_message
            ? IDS_AUTOFILL_SAVE_CARD_PROMPT_UPLOAD_EXPLANATION_V3_WITH_NAME_AND_DEVICE
            : IDS_AUTOFILL_SAVE_CARD_PROMPT_UPLOAD_EXPLANATION_V3_WITH_NAME);
  }

  if (features::ShouldShowImprovedUserConsentForCreditCardSave()) {
    std::string param = base::GetFieldTrialParamValueByFeature(
        features::kAutofillSaveCreditCardUsesImprovedMessaging,
        features::kAutofillSaveCreditCardUsesImprovedMessagingParamName);
    if (param ==
            features::
                kAutofillSaveCreditCardUsesImprovedMessagingParamValueStoreCard ||
        param ==
            features::
                kAutofillSaveCreditCardUsesImprovedMessagingParamValueStoreBillingDetails) {
      return l10n_util::GetStringUTF16(
          offer_to_save_on_device_message
              ? IDS_AUTOFILL_SAVE_CARD_PROMPT_UPLOAD_EXPLANATION_COPY_TEST_STORE_WITH_DEVICE
              : IDS_AUTOFILL_SAVE_CARD_PROMPT_UPLOAD_EXPLANATION_COPY_TEST_STORE);
    }
    if (param ==
        features::
            kAutofillSaveCreditCardUsesImprovedMessagingParamValueAddCard) {
      return l10n_util::GetStringUTF16(
          offer_to_save_on_device_message
              ? IDS_AUTOFILL_SAVE_CARD_PROMPT_UPLOAD_EXPLANATION_COPY_TEST_ADD_CARD_WITH_DEVICE
              : IDS_AUTOFILL_SAVE_CARD_PROMPT_UPLOAD_EXPLANATION_COPY_TEST_ADD_CARD);
    }
    if (param ==
        features::
            kAutofillSaveCreditCardUsesImprovedMessagingParamValueConfirmAndSaveCard) {
      return l10n_util::GetStringUTF16(
          offer_to_save_on_device_message
              ? IDS_AUTOFILL_SAVE_CARD_PROMPT_UPLOAD_EXPLANATION_COPY_TEST_CONFIRM_AND_SAVE_CARD_WITH_DEVICE
              : IDS_AUTOFILL_SAVE_CARD_PROMPT_UPLOAD_EXPLANATION_COPY_TEST_CONFIRM_AND_SAVE_CARD);
    }
  }

  return l10n_util::GetStringUTF16(
      offer_to_save_on_device_message
          ? IDS_AUTOFILL_SAVE_CARD_PROMPT_UPLOAD_EXPLANATION_V3_WITH_DEVICE
          : IDS_AUTOFILL_SAVE_CARD_PROMPT_UPLOAD_EXPLANATION_V3);
}

base::string16 SaveCardBubbleControllerImpl::GetAcceptButtonText() const {
  switch (current_bubble_type_) {
    case BubbleType::LOCAL_SAVE:
      return l10n_util::GetStringUTF16(
          IDS_AUTOFILL_SAVE_CARD_BUBBLE_LOCAL_SAVE_ACCEPT);
    case BubbleType::UPLOAD_SAVE: {
      if (features::ShouldShowImprovedUserConsentForCreditCardSave()) {
        std::string param = base::GetFieldTrialParamValueByFeature(
            features::kAutofillSaveCreditCardUsesImprovedMessaging,
            features::kAutofillSaveCreditCardUsesImprovedMessagingParamName);
        if (param ==
                features::
                    kAutofillSaveCreditCardUsesImprovedMessagingParamValueStoreCard ||
            param ==
                features::
                    kAutofillSaveCreditCardUsesImprovedMessagingParamValueStoreBillingDetails) {
          return l10n_util::GetStringUTF16(
              IDS_AUTOFILL_SAVE_CARD_PROMPT_ACCEPT_COPY_TEST_STORE);
        }
        if (param ==
            features::
                kAutofillSaveCreditCardUsesImprovedMessagingParamValueAddCard) {
          return l10n_util::GetStringUTF16(
              IDS_AUTOFILL_SAVE_CARD_PROMPT_ACCEPT_COPY_TEST_ADD);
        }
        if (param ==
            features::
                kAutofillSaveCreditCardUsesImprovedMessagingParamValueConfirmAndSaveCard) {
          return l10n_util::GetStringUTF16(
              IDS_AUTOFILL_SAVE_CARD_PROMPT_ACCEPT_COPY_TEST_CONFIRM_AND_SAVE);
        }
      }

      return l10n_util::GetStringUTF16(
          IDS_AUTOFILL_SAVE_CARD_BUBBLE_UPLOAD_SAVE_ACCEPT);
    }
    case BubbleType::MANAGE_CARDS:
      return l10n_util::GetStringUTF16(IDS_AUTOFILL_DONE);
    case BubbleType::SIGN_IN_PROMO:
    case BubbleType::FAILURE:
    case BubbleType::INACTIVE:
      NOTREACHED();
      return base::string16();
  }
}

base::string16 SaveCardBubbleControllerImpl::GetDeclineButtonText() const {
  switch (current_bubble_type_) {
    case BubbleType::LOCAL_SAVE:
      return l10n_util::GetStringUTF16(
          IDS_AUTOFILL_NO_THANKS_DESKTOP_LOCAL_SAVE);
    case BubbleType::UPLOAD_SAVE: {
      if (features::ShouldShowImprovedUserConsentForCreditCardSave()) {
        std::string param = base::GetFieldTrialParamValueByFeature(
            features::kAutofillSaveCreditCardUsesImprovedMessaging,
            features::kAutofillSaveCreditCardUsesImprovedMessagingParamName);
        if (param ==
                features::
                    kAutofillSaveCreditCardUsesImprovedMessagingParamValueStoreCard ||
            param ==
                features::
                    kAutofillSaveCreditCardUsesImprovedMessagingParamValueStoreBillingDetails) {
          return l10n_util::GetStringUTF16(
              IDS_AUTOFILL_SAVE_CARD_PROMPT_DECLINE_COPY_TEST_STORE);
        }
        if (param ==
            features::
                kAutofillSaveCreditCardUsesImprovedMessagingParamValueAddCard) {
          return l10n_util::GetStringUTF16(
              IDS_AUTOFILL_SAVE_CARD_PROMPT_DECLINE_COPY_TEST_ADD);
        }
      }

      return l10n_util::GetStringUTF16(
          IDS_AUTOFILL_NO_THANKS_DESKTOP_UPLOAD_SAVE);
    }
    case BubbleType::MANAGE_CARDS:
    case BubbleType::SIGN_IN_PROMO:
    case BubbleType::FAILURE:
    case BubbleType::INACTIVE:
      NOTREACHED();
      return base::string16();
  }
}

const AccountInfo& SaveCardBubbleControllerImpl::GetAccountInfo() const {
  return account_info_;
}

Profile* SaveCardBubbleControllerImpl::GetProfile() const {
  if (!web_contents())
    return nullptr;
  return Profile::FromBrowserContext(web_contents()->GetBrowserContext());
}

const CreditCard& SaveCardBubbleControllerImpl::GetCard() const {
  return card_;
}

bool SaveCardBubbleControllerImpl::ShouldRequestNameFromUser() const {
  return options_.should_request_name_from_user;
}

bool SaveCardBubbleControllerImpl::ShouldRequestExpirationDateFromUser() const {
  return options_.should_request_expiration_date_from_user;
}

bool SaveCardBubbleControllerImpl::ShouldShowSignInPromo() const {
  if (is_upload_save_)
    return false;

  const syncer::SyncService* sync_service =
      ProfileSyncServiceFactory::GetForProfile(GetProfile());

  return !sync_service ||
         sync_service->HasDisableReason(
             syncer::SyncService::DISABLE_REASON_NOT_SIGNED_IN) ||
         sync_service->HasDisableReason(
             syncer::SyncService::DISABLE_REASON_USER_CHOICE);
}

bool SaveCardBubbleControllerImpl::ShouldShowCardSavedAnimation() const {
  return should_show_card_saved_animation_;
}

void SaveCardBubbleControllerImpl::OnSyncPromoAccepted(
    const AccountInfo& account,
    signin_metrics::AccessPoint access_point,
    bool is_default_promo_account) {
  DCHECK(current_bubble_type_ == BubbleType::SIGN_IN_PROMO ||
         current_bubble_type_ == BubbleType::MANAGE_CARDS);
  Browser* browser = chrome::FindBrowserWithWebContents(web_contents());
  signin_ui_util::EnableSyncFromPromo(browser, account, access_point,
                                      is_default_promo_account);
}

void SaveCardBubbleControllerImpl::OnSaveButton(
    const AutofillClient::UserProvidedCardDetails& user_provided_card_details) {
  save_card_bubble_view_ = nullptr;

  switch (current_bubble_type_) {
    case BubbleType::UPLOAD_SAVE: {
      DCHECK(!upload_save_card_prompt_callback_.is_null());

      base::string16 name_provided_by_user;
      if (!user_provided_card_details.cardholder_name.empty()) {
        // Log whether the name was changed by the user or simply accepted
        // without edits.
        AutofillMetrics::LogSaveCardCardholderNameWasEdited(
            user_provided_card_details.cardholder_name !=
            base::UTF8ToUTF16(account_info_.full_name));
        // Trim the cardholder name provided by the user and send it in the
        // callback so it can be included in the final request.
        DCHECK(ShouldRequestNameFromUser());
        base::TrimWhitespace(user_provided_card_details.cardholder_name,
                             base::TRIM_ALL, &name_provided_by_user);
      }
      std::move(upload_save_card_prompt_callback_)
          .Run(AutofillClient::ACCEPTED, user_provided_card_details);
      break;
    }
    case BubbleType::LOCAL_SAVE:
      DCHECK(!local_save_card_prompt_callback_.is_null());
      // Show an animated card saved confirmation message next time
      // UpdateSaveCardIcon() is called.
      should_show_card_saved_animation_ = true;

      std::move(local_save_card_prompt_callback_).Run(AutofillClient::ACCEPTED);
      break;
    case BubbleType::MANAGE_CARDS:
      AutofillMetrics::LogManageCardsPromptMetric(
          AutofillMetrics::MANAGE_CARDS_DONE, is_upload_save_);
      return;
    case BubbleType::SIGN_IN_PROMO:
    case BubbleType::FAILURE:
    case BubbleType::INACTIVE:
      NOTREACHED();
  }

  const BubbleType previous_bubble_type = current_bubble_type_;
  current_bubble_type_ = BubbleType::INACTIVE;

  // If user just saved a card locally, the next bubble can either be a sign-in
  // promo or a manage cards view. If we need to show a sign-in promo, that
  // will be handled by OnAnimationEnded(), otherwise clicking the icon again
  // will show the MANAGE_CARDS bubble, which is set here.
  if (previous_bubble_type == BubbleType::LOCAL_SAVE)
    current_bubble_type_ = BubbleType::MANAGE_CARDS;

  if (previous_bubble_type == BubbleType::LOCAL_SAVE ||
      previous_bubble_type == BubbleType::UPLOAD_SAVE) {
    AutofillMetrics::LogSaveCardPromptMetric(
        AutofillMetrics::SAVE_CARD_PROMPT_END_ACCEPTED, is_upload_save_,
        is_reshow_, options_,
        pref_service_->GetInteger(
            prefs::kAutofillAcceptSaveCreditCardPromptState),
        GetSecurityLevel(), GetSyncState());
    pref_service_->SetInteger(
        prefs::kAutofillAcceptSaveCreditCardPromptState,
        prefs::PREVIOUS_SAVE_CREDIT_CARD_PROMPT_USER_DECISION_ACCEPTED);
  }
}

void SaveCardBubbleControllerImpl::OnCancelButton() {
  if (current_bubble_type_ == BubbleType::LOCAL_SAVE ||
      current_bubble_type_ == BubbleType::UPLOAD_SAVE) {
    AutofillMetrics::LogSaveCardPromptMetric(
        AutofillMetrics::SAVE_CARD_PROMPT_END_DENIED, is_upload_save_,
        is_reshow_, options_,
        pref_service_->GetInteger(
            prefs::kAutofillAcceptSaveCreditCardPromptState),
        GetSecurityLevel(), GetSyncState());
    pref_service_->SetInteger(
        prefs::kAutofillAcceptSaveCreditCardPromptState,
        prefs::PREVIOUS_SAVE_CREDIT_CARD_PROMPT_USER_DECISION_DENIED);

    if (current_bubble_type_ == BubbleType::LOCAL_SAVE) {
      std::move(local_save_card_prompt_callback_).Run(AutofillClient::DECLINED);
    } else {  // BubbleType::UPLOAD_SAVE
      std::move(upload_save_card_prompt_callback_)
          .Run(AutofillClient::DECLINED, {});
    }
  }

  current_bubble_type_ = BubbleType::INACTIVE;
}

void SaveCardBubbleControllerImpl::OnLegalMessageLinkClicked(const GURL& url) {
  OpenUrl(url);
  AutofillMetrics::LogSaveCardPromptMetric(
      AutofillMetrics::SAVE_CARD_PROMPT_DISMISS_CLICK_LEGAL_MESSAGE,
      is_upload_save_, is_reshow_, options_,
      pref_service_->GetInteger(
          prefs::kAutofillAcceptSaveCreditCardPromptState),
      GetSecurityLevel(), GetSyncState());
}

void SaveCardBubbleControllerImpl::OnManageCardsClicked() {
  DCHECK(current_bubble_type_ == BubbleType::MANAGE_CARDS);

  AutofillMetrics::LogManageCardsPromptMetric(
      AutofillMetrics::MANAGE_CARDS_MANAGE_CARDS, is_upload_save_);

  ShowPaymentsSettingsPage();
}

void SaveCardBubbleControllerImpl::ShowPaymentsSettingsPage() {
  chrome::ShowSettingsSubPage(
      chrome::FindBrowserWithWebContents(web_contents()),
      chrome::kPaymentsSubPage);
}

void SaveCardBubbleControllerImpl::OnBubbleClosed() {
  save_card_bubble_view_ = nullptr;
  // Sign-in promo should only be shown once, so if it was displayed presently,
  // reopening the bubble will show the card management bubble.
  if (current_bubble_type_ == BubbleType::SIGN_IN_PROMO)
    current_bubble_type_ = BubbleType::MANAGE_CARDS;
  UpdateSaveCardIcon();
  if (observer_for_testing_)
    observer_for_testing_->OnBubbleClosed();
}

void SaveCardBubbleControllerImpl::OnAnimationEnded() {
  // Do not repeat the animation next time UpdateSaveCardIcon() is called,
  // unless explicitly set somewhere else.
  should_show_card_saved_animation_ = false;

  // We do not want to show the promo if the user clicked on the icon and the
  // manage cards bubble started to show.
  if (!save_card_bubble_view_)
    ShowBubbleForSignInPromo();
}

const LegalMessageLines& SaveCardBubbleControllerImpl::GetLegalMessageLines()
    const {
  return legal_message_lines_;
}

bool SaveCardBubbleControllerImpl::IsUploadSave() const {
  return is_upload_save_;
}

BubbleType SaveCardBubbleControllerImpl::GetBubbleType() const {
  return current_bubble_type_;
}

AutofillSyncSigninState SaveCardBubbleControllerImpl::GetSyncState() const {
  return personal_data_manager_->GetSyncSigninState();
}

void SaveCardBubbleControllerImpl::DidFinishNavigation(
    content::NavigationHandle* navigation_handle) {
  if (!navigation_handle->IsInMainFrame() || !navigation_handle->HasCommitted())
    return;

  // Nothing to do if there's no bubble available.
  if (current_bubble_type_ == BubbleType::INACTIVE)
    return;

  // Don't react to same-document (fragment) navigations.
  if (navigation_handle->IsSameDocument())
    return;

  // Don't do anything if a navigation occurs before a user could reasonably
  // interact with the bubble.
  const base::TimeDelta elapsed_time =
      AutofillClock::Now() - bubble_shown_timestamp_;
  if (elapsed_time < kCardBubbleSurviveNavigationTime)
    return;

  // Otherwise, get rid of the bubble and icon.
  const BubbleType previous_bubble_type = current_bubble_type_;
  current_bubble_type_ = BubbleType::INACTIVE;

  bool bubble_was_visible = save_card_bubble_view_;
  if (bubble_was_visible) {
    save_card_bubble_view_->Hide();
    OnBubbleClosed();
  } else {
    UpdateSaveCardIcon();
  }

  if (previous_bubble_type == BubbleType::LOCAL_SAVE ||
      previous_bubble_type == BubbleType::UPLOAD_SAVE) {
    AutofillMetrics::LogSaveCardPromptMetric(
        bubble_was_visible
            ? AutofillMetrics::SAVE_CARD_PROMPT_END_NAVIGATION_SHOWING
            : AutofillMetrics::SAVE_CARD_PROMPT_END_NAVIGATION_HIDDEN,
        is_upload_save_, is_reshow_, options_,
        pref_service_->GetInteger(
            prefs::kAutofillAcceptSaveCreditCardPromptState),
        GetSecurityLevel(), GetSyncState());

    if (previous_bubble_type == BubbleType::LOCAL_SAVE) {
      DCHECK(!local_save_card_prompt_callback_.is_null());
      std::move(local_save_card_prompt_callback_).Run(AutofillClient::IGNORED);
    } else {  // BubbleType::UPLOAD_SAVE
      DCHECK(!upload_save_card_prompt_callback_.is_null());
      std::move(upload_save_card_prompt_callback_)
          .Run(AutofillClient::IGNORED, {});
    }
  }
}

void SaveCardBubbleControllerImpl::OnVisibilityChanged(
    content::Visibility visibility) {
  if (visibility == content::Visibility::HIDDEN)
    HideBubble();
}

void SaveCardBubbleControllerImpl::WebContentsDestroyed() {
  HideBubble();
}

void SaveCardBubbleControllerImpl::FetchAccountInfo() {
  Profile* profile = GetProfile();
  if (!profile)
    return;
  auto* identity_manager = IdentityManagerFactory::GetForProfile(profile);
  if (!identity_manager)
    return;
  auto* personal_data_manager =
      PersonalDataManagerFactory::GetForProfile(profile);
  if (!personal_data_manager)
    return;
  base::Optional<AccountInfo> account_info =
      identity_manager->FindExtendedAccountInfoForAccount(
          personal_data_manager->GetAccountInfoForPaymentsServer());
  account_info_ = account_info.value_or(AccountInfo{});
}

void SaveCardBubbleControllerImpl::ShowBubble() {
  DCHECK(current_bubble_type_ != BubbleType::INACTIVE);
  // Upload save callback should not be null for UPLOAD_SAVE state.
  DCHECK(!(upload_save_card_prompt_callback_.is_null() &&
           current_bubble_type_ == BubbleType::UPLOAD_SAVE));
  // Local save callback should not be null for LOCAL_SAVE state.
  DCHECK(!(local_save_card_prompt_callback_.is_null() &&
           current_bubble_type_ == BubbleType::LOCAL_SAVE));
  DCHECK(!save_card_bubble_view_);

  // Need to create location bar icon before bubble, otherwise bubble will be
  // unanchored.
  UpdateSaveCardIcon();

  Browser* browser = chrome::FindBrowserWithWebContents(web_contents());
  save_card_bubble_view_ = browser->window()->ShowSaveCreditCardBubble(
      web_contents(), this, is_reshow_);
  DCHECK(save_card_bubble_view_);

  // Update icon after creating |save_card_bubble_view_| so that icon will show
  // its "toggled on" state.
  UpdateSaveCardIcon();

  bubble_shown_timestamp_ = AutofillClock::Now();

  switch (current_bubble_type_) {
    case BubbleType::UPLOAD_SAVE:
    case BubbleType::LOCAL_SAVE:
      AutofillMetrics::LogSaveCardPromptMetric(
          AutofillMetrics::SAVE_CARD_PROMPT_SHOWN, is_upload_save_, is_reshow_,
          options_,
          pref_service_->GetInteger(
              prefs::kAutofillAcceptSaveCreditCardPromptState),
          GetSecurityLevel(), GetSyncState());
      break;
    case BubbleType::MANAGE_CARDS:
      AutofillMetrics::LogManageCardsPromptMetric(
          AutofillMetrics::MANAGE_CARDS_SHOWN, is_upload_save_);
      break;
    case BubbleType::SIGN_IN_PROMO:
      break;
    case BubbleType::FAILURE:
      // TODO(crbug.com/964127): Add metrics.
      break;
    case BubbleType::INACTIVE:
      NOTREACHED();
  }

  if (observer_for_testing_) {
    observer_for_testing_->OnBubbleShown();
  }
}

void SaveCardBubbleControllerImpl::ShowIconOnly() {
  DCHECK(current_bubble_type_ != BubbleType::INACTIVE);
  // Upload save callback should not be null for UPLOAD_SAVE state.
  DCHECK(!(upload_save_card_prompt_callback_.is_null() &&
           current_bubble_type_ == BubbleType::UPLOAD_SAVE));
  // Local save callback should not be null for LOCAL_SAVE state.
  DCHECK(!(local_save_card_prompt_callback_.is_null() &&
           current_bubble_type_ == BubbleType::LOCAL_SAVE));
  DCHECK(!save_card_bubble_view_);

  // Show the icon only. The bubble can still be displayed if the user
  // explicitly clicks the icon.
  UpdateSaveCardIcon();

  bubble_shown_timestamp_ = AutofillClock::Now();

  switch (current_bubble_type_) {
    case BubbleType::UPLOAD_SAVE:
    case BubbleType::LOCAL_SAVE:
      AutofillMetrics::LogSaveCardPromptMetric(
          AutofillMetrics::SAVE_CARD_ICON_SHOWN_WITHOUT_PROMPT, is_upload_save_,
          is_reshow_, options_,
          pref_service_->GetInteger(
              prefs::kAutofillAcceptSaveCreditCardPromptState),
          GetSecurityLevel(), GetSyncState());
      break;
    case BubbleType::FAILURE:
      // TODO(crbug.com/964127): Add metrics.
      break;
    case BubbleType::MANAGE_CARDS:
    case BubbleType::SIGN_IN_PROMO:
    case BubbleType::INACTIVE:
      NOTREACHED();
  }
}

void SaveCardBubbleControllerImpl::UpdateSaveCardIcon() {
  ::autofill::UpdatePageActionIcon(PageActionIconType::kSaveCard,
                                   web_contents());
}

void SaveCardBubbleControllerImpl::OpenUrl(const GURL& url) {
  web_contents()->OpenURL(content::OpenURLParams(
      url, content::Referrer(), WindowOpenDisposition::NEW_FOREGROUND_TAB,
      ui::PAGE_TRANSITION_LINK, false));
}

security_state::SecurityLevel SaveCardBubbleControllerImpl::GetSecurityLevel()
    const {
  return security_level_;
}

WEB_CONTENTS_USER_DATA_KEY_IMPL(SaveCardBubbleControllerImpl)

}  // namespace autofill
