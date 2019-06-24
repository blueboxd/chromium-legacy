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
#include "components/autofill/core/browser/data_model/credit_card.h"
#include "components/autofill/core/common/autofill_features.h"
#include "components/strings/grit/components_strings.h"
#include "ui/base/l10n/l10n_util.h"

namespace autofill {

namespace {

base::string16 GetTitle(bool unused) {
  return l10n_util::GetStringUTF16(IDS_MANUAL_FILLING_CREDIT_CARD_SHEET_TITLE);
}

void AddField(const base::string16& data, UserInfo* user_info) {
  user_info->add_field(UserInfo::Field(data, data,
                                       /*is_password=*/false,
                                       /*selectable=*/true));
}

UserInfo TranslateCard(const CreditCard* data) {
  DCHECK(data);

  UserInfo user_info;

  AddField(data->ObfuscatedLastFourDigits(), &user_info);

  if (data->HasValidExpirationDate()) {
    AddField(data->ExpirationMonthAsString(), &user_info);
    AddField(data->Expiration4DigitYearAsString(), &user_info);
  } else {
    AddField(base::string16(), &user_info);
    AddField(base::string16(), &user_info);
  }

  if (data->HasNameOnCard()) {
    AddField(data->GetRawInfo(autofill::CREDIT_CARD_NAME_FULL), &user_info);
  } else {
    AddField(base::string16(), &user_info);
  }

  return user_info;
}

}  // namespace

CreditCardAccessoryControllerImpl::~CreditCardAccessoryControllerImpl() =
    default;

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
  const std::vector<CreditCard*> suggestions = GetSuggestions();
  std::vector<UserInfo> info_to_add;
  std::transform(suggestions.begin(), suggestions.end(),
                 std::back_inserter(info_to_add), &TranslateCard);

  const std::vector<FooterCommand> footer_commands = {FooterCommand(
      l10n_util::GetStringUTF16(IDS_MANUAL_FILLING_CREDIT_CARD_SHEET_TITLE),
      AccessoryAction::MANAGE_CREDIT_CARDS)};

  bool has_suggestions = !info_to_add.empty();
  GetManualFillingController()->RefreshSuggestions(
      autofill::CreateAccessorySheetData(
          AccessoryTabType::CREDIT_CARDS, GetTitle(has_suggestions),
          std::move(info_to_add), std::move(footer_commands)));
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
      personal_data_manager_for_testing_(nullptr) {}

CreditCardAccessoryControllerImpl::CreditCardAccessoryControllerImpl(
    content::WebContents* web_contents,
    base::WeakPtr<ManualFillingController> mf_controller,
    PersonalDataManager* personal_data_manager)
    : web_contents_(web_contents),
      mf_controller_(mf_controller),
      personal_data_manager_for_testing_(personal_data_manager) {}

const std::vector<CreditCard*>
CreditCardAccessoryControllerImpl::GetSuggestions() {
  const PersonalDataManager* personal_data_manager =
      personal_data_manager_for_testing_;
  if (!personal_data_manager) {
    personal_data_manager = autofill::PersonalDataManagerFactory::GetForProfile(
        Profile::FromBrowserContext(web_contents_->GetBrowserContext()));
  }
  if (!personal_data_manager) {
    return {};  // No data available.
  }
  return personal_data_manager->GetCreditCardsToSuggest(
      /*include_server_cards=*/true);
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
