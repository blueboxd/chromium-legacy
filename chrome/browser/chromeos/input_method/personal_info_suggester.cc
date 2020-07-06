// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/input_method/personal_info_suggester.h"
#include "chrome/browser/chromeos/extensions/input_method_api.h"
#include "chrome/browser/extensions/api/input_ime/input_ime_api.h"

#include "ash/public/cpp/ash_pref_names.h"
#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/autofill/personal_data_manager_factory.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/chromeos/input_method/ui/suggestion_details.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/ui/ash/keyboard/chrome_keyboard_controller_client.h"
#include "chromeos/constants/chromeos_pref_names.h"
#include "components/autofill/core/browser/data_model/autofill_profile.h"
#include "components/autofill/core/browser/personal_data_manager.h"
#include "components/autofill/core/browser/ui/label_formatter_utils.h"
#include "components/prefs/pref_service.h"
#include "components/prefs/scoped_user_pref_update.h"

namespace chromeos {

namespace {

const size_t kMaxConfirmedTextLength = 10;
const char kAssistEmailPrefix[] = "my email is ";
const char kAssistNamePrefix[] = "my name is ";
const char kAssistAddressPrefix[] = "my address is ";
const char kAssistPhoneNumberPrefix[] = "my phone number is ";
const char kAssistNumberPrefix[] = "my number is ";
const char kAssistFirstNamePrefix[] = "my first name is ";
const char kAssistLastNamePrefix[] = "my last name is ";
const char kAnnounceShowTab[] = "Press tab to insert.";

constexpr base::TimeDelta kTtsShowDelay =
    base::TimeDelta::FromMilliseconds(1200);

const std::vector<autofill::ServerFieldType>& GetHomeAddressTypes() {
  static base::NoDestructor<std::vector<autofill::ServerFieldType>>
      homeAddressTypes{
          {autofill::ServerFieldType::ADDRESS_HOME_LINE1,
           autofill::ServerFieldType::ADDRESS_HOME_LINE2,
           autofill::ServerFieldType::ADDRESS_HOME_LINE3,
           autofill::ServerFieldType::ADDRESS_HOME_STREET_ADDRESS,
           autofill::ServerFieldType::ADDRESS_HOME_DEPENDENT_LOCALITY,
           autofill::ServerFieldType::ADDRESS_HOME_CITY,
           autofill::ServerFieldType::ADDRESS_HOME_STATE,
           autofill::ServerFieldType::ADDRESS_HOME_ZIP,
           autofill::ServerFieldType::ADDRESS_HOME_SORTING_CODE,
           autofill::ServerFieldType::ADDRESS_HOME_COUNTRY}};
  return *homeAddressTypes;
}

}  // namespace

TtsHandler::TtsHandler(Profile* profile) : profile_(profile) {}
TtsHandler::~TtsHandler() = default;

void TtsHandler::Announce(const std::string& text,
                          const base::TimeDelta delay) {
  const bool chrome_vox_enabled = profile_->GetPrefs()->GetBoolean(
      ash::prefs::kAccessibilitySpokenFeedbackEnabled);
  if (!chrome_vox_enabled)
    return;

  delay_timer_ = std::make_unique<base::OneShotTimer>();
  delay_timer_->Start(
      FROM_HERE, delay,
      base::BindOnce(&TtsHandler::Speak, base::Unretained(this), text));
}

void TtsHandler::OnTtsEvent(content::TtsUtterance* utterance,
                            content::TtsEventType event_type,
                            int char_index,
                            int length,
                            const std::string& error_message) {}

void TtsHandler::Speak(const std::string& text) {
  std::unique_ptr<content::TtsUtterance> utterance =
      content::TtsUtterance::Create(profile_);
  utterance->SetText(text);
  utterance->SetEventDelegate(this);

  auto* tts_controller = content::TtsController::GetInstance();
  tts_controller->Stop();
  tts_controller->SpeakOrEnqueue(std::move(utterance));
}

AssistiveType ProposeAssistiveAction(const base::string16& text) {
  AssistiveType action = AssistiveType::kGenericAction;
  if (base::EndsWith(text, base::UTF8ToUTF16(kAssistEmailPrefix),
                     base::CompareCase::INSENSITIVE_ASCII)) {
    action = AssistiveType::kPersonalEmail;
  }
  if (base::EndsWith(text, base::UTF8ToUTF16(kAssistNamePrefix),
                     base::CompareCase::INSENSITIVE_ASCII)) {
    action = AssistiveType::kPersonalName;
  }
  if (base::EndsWith(text, base::UTF8ToUTF16(kAssistAddressPrefix),
                     base::CompareCase::INSENSITIVE_ASCII)) {
    action = AssistiveType::kPersonalAddress;
  }
  if (base::EndsWith(text, base::UTF8ToUTF16(kAssistPhoneNumberPrefix),
                     base::CompareCase::INSENSITIVE_ASCII)) {
    action = AssistiveType::kPersonalPhoneNumber;
  }
  if (base::EndsWith(text, base::UTF8ToUTF16(kAssistNumberPrefix),
                     base::CompareCase::INSENSITIVE_ASCII)) {
    action = AssistiveType::kPersonalNumber;
  }
  if (base::EndsWith(text, base::UTF8ToUTF16(kAssistFirstNamePrefix),
                     base::CompareCase::INSENSITIVE_ASCII)) {
    action = AssistiveType::kPersonalFirstName;
  }
  if (base::EndsWith(text, base::UTF8ToUTF16(kAssistLastNamePrefix),
                     base::CompareCase::INSENSITIVE_ASCII)) {
    action = AssistiveType::kPersonalLastName;
  }
  return action;
}

PersonalInfoSuggester::PersonalInfoSuggester(
    SuggestionHandlerInterface* suggestion_handler,
    Profile* profile,
    autofill::PersonalDataManager* personal_data_manager,
    std::unique_ptr<TtsHandler> tts_handler)
    : suggestion_handler_(suggestion_handler),
      profile_(profile),
      personal_data_manager_(
          personal_data_manager
              ? personal_data_manager
              : autofill::PersonalDataManagerFactory::GetForProfile(profile)),
      tts_handler_(tts_handler ? std::move(tts_handler)
                               : std::make_unique<TtsHandler>(profile)) {}

PersonalInfoSuggester::~PersonalInfoSuggester() {}

void PersonalInfoSuggester::OnFocus(int context_id) {
  context_id_ = context_id;
}

void PersonalInfoSuggester::OnBlur() {
  context_id_ = -1;
}

SuggestionStatus PersonalInfoSuggester::HandleKeyEvent(
    const InputMethodEngineBase::KeyboardEvent& event) {
  if (suggestion_shown_) {
    if (event.key == "Tab" || event.key == "Right") {
      if (AcceptSuggestion()) {
        IncrementPrefValueTilCapped(kPersonalInfoSuggesterTabAcceptanceCount,
                                    kMaxTabAcceptanceCount);
        return SuggestionStatus::kAccept;
      }
    } else if (event.key == "Esc") {
      DismissSuggestion();
      return SuggestionStatus::kDismiss;
    }
  }
  return SuggestionStatus::kNotHandled;
}

bool PersonalInfoSuggester::Suggest(const base::string16& text) {
  if (suggestion_shown_) {
    size_t text_length = text.length();
    bool matched = false;
    for (size_t offset = 0;
         offset < suggestion_.length() && offset < text_length &&
         offset < kMaxConfirmedTextLength;
         offset++) {
      base::string16 text_before = text.substr(0, text_length - offset);
      base::string16 confirmed_text = text.substr(text_length - offset);
      if (base::StartsWith(suggestion_, confirmed_text,
                           base::CompareCase::INSENSITIVE_ASCII) &&
          suggestion_ == GetSuggestion(text_before)) {
        matched = true;
        ShowSuggestion(suggestion_, offset);
        break;
      }
    }
    return matched;
  } else {
    suggestion_ = GetSuggestion(text);
    if (!suggestion_.empty())
      ShowSuggestion(suggestion_, 0);
    return suggestion_shown_;
  }
}

base::string16 PersonalInfoSuggester::GetSuggestion(
    const base::string16& text) {
  proposed_action_type_ = ProposeAssistiveAction(text);

  if (proposed_action_type_ == AssistiveType::kGenericAction)
    return base::EmptyString16();

  if (proposed_action_type_ == AssistiveType::kPersonalEmail) {
    return profile_ ? base::UTF8ToUTF16(profile_->GetProfileUserName())
                    : base::EmptyString16();
  }

  if (!personal_data_manager_)
    return base::EmptyString16();

  auto autofill_profiles = personal_data_manager_->GetProfilesToSuggest();
  if (autofill_profiles.empty())
    return base::EmptyString16();

  // Currently, we are just picking the first candidate, will improve the
  // strategy in the future.
  auto* profile = autofill_profiles[0];
  base::string16 suggestion;
  const std::string app_locale = g_browser_process->GetApplicationLocale();
  switch (proposed_action_type_) {
    case AssistiveType::kPersonalName:
      suggestion = profile->GetRawInfo(autofill::ServerFieldType::NAME_FULL);
      break;
    case AssistiveType::kPersonalAddress:
      suggestion = autofill::GetLabelNationalAddress(GetHomeAddressTypes(),
                                                     *profile, app_locale);
      break;
    case AssistiveType::kPersonalPhoneNumber:
    case AssistiveType::kPersonalNumber:
      suggestion = profile->GetRawInfo(
          autofill::ServerFieldType::PHONE_HOME_WHOLE_NUMBER);
      break;
    case AssistiveType::kPersonalFirstName:
      suggestion = profile->GetRawInfo(autofill::ServerFieldType::NAME_FIRST);
      break;
    case AssistiveType::kPersonalLastName:
      suggestion = profile->GetRawInfo(autofill::ServerFieldType::NAME_LAST);
      break;
    default:
      NOTREACHED();
      break;
  }
  return suggestion;
}

void PersonalInfoSuggester::ShowSuggestion(const base::string16& text,
                                           const size_t confirmed_length) {
  auto* keyboard_client = ChromeKeyboardControllerClient::Get();
  if (keyboard_client->is_keyboard_enabled()) {
    const std::vector<std::string> args{base::UTF16ToUTF8(text)};
    suggestion_handler_->OnSuggestionsChanged(args);
    return;
  }

  std::string error;
  bool show_tab = GetPrefValue(kPersonalInfoSuggesterTabAcceptanceCount) <
                  kMaxTabAcceptanceCount;
  ui::ime::SuggestionDetails details;
  details.text = text;
  details.confirmed_length = confirmed_length;
  details.show_tab = show_tab;
  details.show_setting_link =
      GetPrefValue(kPersonalInfoSuggesterTabAcceptanceCount) == 0 &&
      GetPrefValue(kPersonalInfoSuggesterShowSettingCount) <
          kMaxShowSettingCount;
  suggestion_handler_->SetSuggestion(context_id_, details, &error);
  if (!error.empty()) {
    LOG(ERROR) << "Fail to show suggestion. " << error;
  }

  if (suggestion_shown_) {
    first_shown_ = false;
  } else {
    first_shown_ = true;
    IncrementPrefValueTilCapped(kPersonalInfoSuggesterShowSettingCount,
                                kMaxShowSettingCount);
    tts_handler_->Announce(
        // TODO(jiwan): Add translation to other languages when we support more
        // than English.
        base::StringPrintf(
            "Suggested text %s. %s", base::UTF16ToUTF8(text).c_str(),
            show_tab ? kAnnounceShowTab : base::EmptyString().c_str()),
        kTtsShowDelay);
  }

  suggestion_shown_ = true;
}

int PersonalInfoSuggester::GetPrefValue(const std::string& pref_name) {
  DictionaryPrefUpdate update(profile_->GetPrefs(),
                              prefs::kAssistiveInputFeatureSettings);
  auto value = update->FindIntKey(pref_name);
  if (!value.has_value()) {
    update->SetIntKey(pref_name, 0);
    return 0;
  }
  return *value;
}

void PersonalInfoSuggester::IncrementPrefValueTilCapped(
    const std::string& pref_name,
    int max_value) {
  int value = GetPrefValue(pref_name);
  if (value < max_value) {
    DictionaryPrefUpdate update(profile_->GetPrefs(),
                                prefs::kAssistiveInputFeatureSettings);
    update->SetIntKey(pref_name, value + 1);
  }
}

AssistiveType PersonalInfoSuggester::GetProposeActionType() {
  return proposed_action_type_;
}

bool PersonalInfoSuggester::AcceptSuggestion(size_t index) {
  std::string error;
  suggestion_handler_->AcceptSuggestion(context_id_, &error);

  if (!error.empty()) {
    LOG(ERROR) << "Failed to accept suggestion. " << error;
    return false;
  }

  suggestion_shown_ = false;
  tts_handler_->Announce(base::StringPrintf(
      "Inserted suggestion %s.", base::UTF16ToUTF8(suggestion_).c_str()));

  return true;
}

void PersonalInfoSuggester::DismissSuggestion() {
  std::string error;
  suggestion_shown_ = false;
  suggestion_handler_->DismissSuggestion(context_id_, &error);
  if (!error.empty()) {
    LOG(ERROR) << "Failed to dismiss suggestion. " << error;
  }
}

}  // namespace chromeos
