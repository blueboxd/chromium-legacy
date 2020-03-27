// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/input_method/personal_info_suggester.h"

#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/autofill/personal_data_manager_factory.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "components/autofill/core/browser/data_model/autofill_profile.h"
#include "components/autofill/core/browser/personal_data_manager.h"

using input_method::InputMethodEngineBase;

namespace chromeos {

namespace {

const char kAssistEmailPrefix[] = "my email is ";
const char kAssistNamePrefix[] = "my name is ";
const char kAssistAddressPrefix[] = "my address is ";
const char kAssistPhoneNumberPrefix[] = "my phone number is ";

}  // namespace

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
  return action;
}

PersonalInfoSuggester::PersonalInfoSuggester(InputMethodEngine* engine,
                                             Profile* profile)
    : engine_(engine),
      profile_(profile),
      personal_data_manager_(
          autofill::PersonalDataManagerFactory::GetForProfile(profile)) {}

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
    suggestion_shown_ = false;
    if (event.key == "Tab" || event.key == "Right") {
      std::string error;
      engine_->AcceptSuggestion(context_id_, &error);
      return SuggestionStatus::kAccept;
    }
    DismissSuggestion();
    return SuggestionStatus::kDismiss;
  }
  return SuggestionStatus::kNotHandled;
}

bool PersonalInfoSuggester::Suggest(const base::string16& text) {
  base::string16 suggestion_text = GetPersonalInfoSuggestion(text);
  if (!suggestion_text.empty()) {
    ShowSuggestion(suggestion_text);
    suggestion_shown_ = true;
  }
  return suggestion_shown_;
}

base::string16 PersonalInfoSuggester::GetPersonalInfoSuggestion(
    const base::string16& text) {
  AssistiveType action = ProposeAssistiveAction(text);
  proposed_action_type_ = action;

  if (action == AssistiveType::kGenericAction)
    return base::EmptyString16();

  if (action == AssistiveType::kPersonalEmail)
    return base::UTF8ToUTF16(profile_->GetProfileUserName());

  auto autofill_profiles = personal_data_manager_->GetProfilesToSuggest();
  if (autofill_profiles.empty())
    return base::EmptyString16();

  // Currently, we are just picking the first candidate, will improve the
  // strategy in the future.
  auto* data = autofill_profiles[0];
  base::string16 suggestion;
  switch (action) {
    case AssistiveType::kPersonalName:
      suggestion = data->GetRawInfo(autofill::ServerFieldType::NAME_FULL);
      break;
    case AssistiveType::kPersonalAddress:
      suggestion = data->GetRawInfo(
          autofill::ServerFieldType::ADDRESS_HOME_STREET_ADDRESS);
      break;
    case AssistiveType::kPersonalPhoneNumber:
      suggestion =
          data->GetRawInfo(autofill::ServerFieldType::PHONE_HOME_WHOLE_NUMBER);
      break;
    default:
      NOTREACHED();
      break;
  }
  return suggestion;
}

void PersonalInfoSuggester::ShowSuggestion(const base::string16& text) {
  std::string error;
  engine_->SetSuggestion(context_id_, text, &error);
  if (!error.empty()) {
    LOG(ERROR) << "Fail to show suggestion. " << error;
  }
}

AssistiveType PersonalInfoSuggester::GetProposeActionType() {
  return proposed_action_type_;
}

void PersonalInfoSuggester::DismissSuggestion() {
  std::string error;
  suggestion_shown_ = false;
  engine_->DismissSuggestion(context_id_, &error);
  if (!error.empty()) {
    LOG(ERROR) << "Failed to dismiss suggestion. " << error;
  }
}

}  // namespace chromeos
