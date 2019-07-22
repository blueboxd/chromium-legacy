// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/autofill/credit_card_accessory_controller_impl.h"

#include <algorithm>
#include <iterator>
#include <utility>
#include <vector>

#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/android/preferences/preferences_launcher.h"
#include "chrome/browser/autofill/manual_filling_controller.h"
#include "chrome/browser/autofill/manual_filling_utils.h"
#include "chrome/browser/autofill/personal_data_manager_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/vr/vr_tab_helper.h"
#include "components/autofill/content/browser/content_autofill_driver.h"
#include "components/autofill/core/browser/data_model/credit_card.h"
#include "components/autofill/core/common/autofill_features.h"
#include "components/strings/grit/components_strings.h"
#include "ui/base/l10n/l10n_util.h"

namespace autofill {

namespace {

base::string16 GetTitle(bool has_suggestions) {
  return l10n_util::GetStringUTF16(
      has_suggestions ? IDS_MANUAL_FILLING_CREDIT_CARD_SHEET_TITLE
                      : IDS_MANUAL_FILLING_CREDIT_CARD_SHEET_EMPTY_MESSAGE);
}

void AddSimpleField(const base::string16& data, UserInfo* user_info) {
  user_info->add_field(UserInfo::Field(data, data,
                                       /*is_password=*/false,
                                       /*selectable=*/true));
}

UserInfo TranslateCard(const CreditCard* data) {
  DCHECK(data);

  UserInfo user_info;

  base::string16 obfuscated_number = data->ObfuscatedLastFourDigits();
  user_info.add_field(UserInfo::Field(obfuscated_number, obfuscated_number,
                                      data->guid(), /*is_password=*/false,
                                      /*selectable=*/true));

  if (data->HasValidExpirationDate()) {
    AddSimpleField(data->ExpirationMonthAsString(), &user_info);
    AddSimpleField(data->Expiration4DigitYearAsString(), &user_info);
  } else {
    AddSimpleField(base::string16(), &user_info);
    AddSimpleField(base::string16(), &user_info);
  }

  if (data->HasNameOnCard()) {
    AddSimpleField(data->GetRawInfo(autofill::CREDIT_CARD_NAME_FULL),
                   &user_info);
  } else {
    AddSimpleField(base::string16(), &user_info);
  }

  return user_info;
}

}  // namespace

CreditCardAccessoryControllerImpl::~CreditCardAccessoryControllerImpl() {
  if (personal_data_manager_)
    personal_data_manager_->RemoveObserver(this);
}

void CreditCardAccessoryControllerImpl::OnFillingTriggered(
    const UserInfo::Field& selection) {
  autofill::ContentAutofillDriver* driver =
      autofill::ContentAutofillDriver::GetForRenderFrameHost(
          web_contents_->GetFocusedFrame());
  if (!driver)
    return;

  // Credit card number fields have a GUID populated to allow deobfuscation
  // before filling.
  if (selection.id().empty()) {
    driver->RendererShouldFillFieldWithValue(selection.display_text());
    return;
  }

  auto card_iter = std::find_if(cards_cache_.begin(), cards_cache_.end(),
                                [&selection](const auto* card) {
                                  return card->guid() == selection.id();
                                });

  DCHECK(card_iter != cards_cache_.end())
      << "Tried to fill card with unknown GUID";

  CreditCard* matching_card = *card_iter;
  if (matching_card->record_type() ==
      CreditCard::RecordType::MASKED_SERVER_CARD) {
    // Unmasking server cards is not yet supported
    return;
  }
  driver->RendererShouldFillFieldWithValue(matching_card->number());
}

void CreditCardAccessoryControllerImpl::OnOptionSelected(
    AccessoryAction selected_action) {
  if (selected_action == AccessoryAction::MANAGE_CREDIT_CARDS) {
    chrome::android::PreferencesLauncher::ShowAutofillCreditCardSettings(
        web_contents_);
    return;
  }
  NOTREACHED() << "Unhandled selected action: "
               << static_cast<int>(selected_action);
}

// static
bool CreditCardAccessoryController::AllowedForWebContents(
    content::WebContents* web_contents) {
  DCHECK(web_contents) << "Need valid WebContents to attach controller to!";
  if (vr::VrTabHelper::IsInVr(web_contents)) {
    return false;  // TODO(crbug.com/902305): Re-enable if possible.
  }
  return base::FeatureList::IsEnabled(
             autofill::features::kAutofillKeyboardAccessory) &&
         base::FeatureList::IsEnabled(
             autofill::features::kAutofillManualFallbackAndroid);
}

// static
CreditCardAccessoryController* CreditCardAccessoryController::GetOrCreate(
    content::WebContents* web_contents) {
  DCHECK(CreditCardAccessoryController::AllowedForWebContents(web_contents));

  CreditCardAccessoryControllerImpl::CreateForWebContents(web_contents);
  return CreditCardAccessoryControllerImpl::FromWebContents(web_contents);
}

// static
CreditCardAccessoryController* CreditCardAccessoryController::GetIfExisting(
    content::WebContents* web_contents) {
  return CreditCardAccessoryControllerImpl::FromWebContents(web_contents);
}

void CreditCardAccessoryControllerImpl::RefreshSuggestions() {
  FetchSuggestionsFromPersonalDataManager();
  std::vector<UserInfo> info_to_add;
  std::transform(cards_cache_.begin(), cards_cache_.end(),
                 std::back_inserter(info_to_add), &TranslateCard);

  const std::vector<FooterCommand> footer_commands = {FooterCommand(
      l10n_util::GetStringUTF16(
          IDS_MANUAL_FILLING_CREDIT_CARD_SHEET_ALL_ADDRESSES_LINK),
      AccessoryAction::MANAGE_CREDIT_CARDS)};

  bool has_suggestions = !info_to_add.empty();

  GetManualFillingController()->RefreshSuggestions(
      autofill::CreateAccessorySheetData(
          AccessoryTabType::CREDIT_CARDS, GetTitle(has_suggestions),
          std::move(info_to_add), std::move(footer_commands)));
}

void CreditCardAccessoryControllerImpl::OnPersonalDataChanged() {
  RefreshSuggestions();
}

// static
void CreditCardAccessoryControllerImpl::CreateForWebContentsForTesting(
    content::WebContents* web_contents,
    base::WeakPtr<ManualFillingController> mf_controller,
    autofill::PersonalDataManager* personal_data_manager) {
  DCHECK(web_contents) << "Need valid WebContents to attach controller to!";
  DCHECK(!FromWebContents(web_contents)) << "Controller already attached!";
  DCHECK(mf_controller);

  web_contents->SetUserData(
      UserDataKey(),
      base::WrapUnique(new CreditCardAccessoryControllerImpl(
          web_contents, std::move(mf_controller), personal_data_manager)));
}

CreditCardAccessoryControllerImpl::CreditCardAccessoryControllerImpl(
    content::WebContents* web_contents)
    : web_contents_(web_contents),
      personal_data_manager_(
          autofill::PersonalDataManagerFactory::GetForProfile(
              Profile::FromBrowserContext(
                  web_contents_->GetBrowserContext()))) {
  if (personal_data_manager_)
    personal_data_manager_->AddObserver(this);
}

CreditCardAccessoryControllerImpl::CreditCardAccessoryControllerImpl(
    content::WebContents* web_contents,
    base::WeakPtr<ManualFillingController> mf_controller,
    PersonalDataManager* personal_data_manager)
    : web_contents_(web_contents),
      mf_controller_(mf_controller),
      personal_data_manager_(personal_data_manager) {
  if (personal_data_manager_)
    personal_data_manager_->AddObserver(this);
}

void CreditCardAccessoryControllerImpl::
    FetchSuggestionsFromPersonalDataManager() {
  if (!personal_data_manager_) {
    cards_cache_.clear();  // No data available.
  } else {
    cards_cache_ = personal_data_manager_->GetCreditCardsToSuggest(
        /*include_server_cards=*/true);
  }
}

base::WeakPtr<ManualFillingController>
CreditCardAccessoryControllerImpl::GetManualFillingController() {
  if (!mf_controller_)
    mf_controller_ = ManualFillingController::GetOrCreate(web_contents_);
  DCHECK(mf_controller_);
  return mf_controller_;
}

WEB_CONTENTS_USER_DATA_KEY_IMPL(CreditCardAccessoryControllerImpl)

}  // namespace autofill
