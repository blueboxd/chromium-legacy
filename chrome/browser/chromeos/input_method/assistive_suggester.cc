// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/input_method/assistive_suggester.h"

#include "base/feature_list.h"
#include "base/metrics/histogram_functions.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/autofill/personal_data_manager_factory.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chromeos/constants/chromeos_features.h"
#include "components/autofill/core/browser/data_model/autofill_profile.h"
#include "components/autofill/core/browser/personal_data_manager.h"

using input_method::InputMethodEngineBase;

namespace chromeos {

namespace {

const char kMaxTextBeforeCursorLength = 50;
const char kKeydown[] = "keydown";
const char kAssistEmailPrefix[] = "my email is ";
const char kAssistNamePrefix[] = "my name is ";
const char kAssistAddressPrefix[] = "my address is ";
const char kAssistPhoneNumberPrefix[] = "my phone number is ";

// Must match with IMEAssistiveAction in enums.xml
enum class AssistiveType {
  kGenericAction = 0,
  kPersonalEmail = 1,
  kPersonalAddress = 2,
  kPersonalPhoneNumber = 3,
  kPersonalName = 4,
  kMaxValue = kPersonalName,
};

void RecordAssitiveConverage(AssistiveType type) {
  base::UmaHistogramEnumeration("InputMethod.Assistive.Coverage", type);
}

AssistiveType ProposeAssistiveAction(const std::string& text) {
  AssistiveType action = AssistiveType::kGenericAction;
  if (base::EndsWith(text, kAssistEmailPrefix,
                     base::CompareCase::INSENSITIVE_ASCII)) {
    action = AssistiveType::kPersonalEmail;
  }
  if (base::EndsWith(text, kAssistNamePrefix,
                     base::CompareCase::INSENSITIVE_ASCII)) {
    action = AssistiveType::kPersonalName;
  }
  if (base::EndsWith(text, kAssistAddressPrefix,
                     base::CompareCase::INSENSITIVE_ASCII)) {
    action = AssistiveType::kPersonalAddress;
  }
  if (base::EndsWith(text, kAssistPhoneNumberPrefix,
                     base::CompareCase::INSENSITIVE_ASCII)) {
    action = AssistiveType::kPersonalPhoneNumber;
  }
  return action;
}

}  // namespace

AssistiveSuggester::AssistiveSuggester(InputMethodEngine* engine,
                                       Profile* profile)
    : engine_(engine), profile_(profile) {
  personal_data_manager_ =
      autofill::PersonalDataManagerFactory::GetForProfile(profile_);
}

void AssistiveSuggester::OnFocus(int context_id) {
  context_id_ = context_id;
}

void AssistiveSuggester::OnBlur() {
  context_id_ = -1;
}

bool AssistiveSuggester::OnKeyEvent(
    const InputMethodEngineBase::KeyboardEvent& event) {
  if (context_id_ == -1)
    return false;

  // If the user pressed Tab after we show suggestion, we adopt the suggestion,
  // otherwise we dismiss it.
  // We only track keydown event because the suggesting action is triggered by
  // surrounding text change, which is triggered by a keydown event. As a
  // result, the next key event after suggesting would be a keyup event of the
  // same key, and that event is meaningless to us.
  if (suggestion_shown_ && event.type == kKeydown) {
    suggestion_shown_ = false;
    if (event.key == "Tab" || event.key == "Right") {
      engine_->ConfirmCompositionText(false, false);
      return true;
    } else {
      DismissSuggestion();
      suggestion_dismissed_ = true;
    }
  }

  return false;
}

void AssistiveSuggester::RecordAssitiveCoverageMetrics(const std::string& text,
                                                       int cursor_pos,
                                                       int anchor_pos) {
  int len = static_cast<int>(text.length());
  if (cursor_pos > 0 && cursor_pos <= len && cursor_pos == anchor_pos &&
      (cursor_pos == len || base::IsAsciiWhitespace(text[cursor_pos]))) {
    int start_pos = std::max(0, cursor_pos - kMaxTextBeforeCursorLength);
    std::string text_before_cursor =
        text.substr(start_pos, cursor_pos - start_pos);
    AssistiveType action = ProposeAssistiveAction(text_before_cursor);
    if (action != AssistiveType::kGenericAction)
      RecordAssitiveConverage(action);
  }
}

bool AssistiveSuggester::OnSurroundingTextChanged(const std::string& text,
                                                  int cursor_pos,
                                                  int anchor_pos) {
  if (suggestion_dismissed_) {
    suggestion_dismissed_ = false;
    return false;
  }

  if (context_id_ == -1)
    return false;

  bool surrounding_text_changed = false;
  if (!suggestion_shown_) {
    Suggest(text, cursor_pos, anchor_pos);
    surrounding_text_changed = suggestion_shown_;
  }
  return surrounding_text_changed;
}

void AssistiveSuggester::Suggest(const std::string& text,
                                 int cursor_pos,
                                 int anchor_pos) {
  int len = static_cast<int>(text.length());
  if (cursor_pos > 0 && cursor_pos <= len &&
      base::IsAsciiWhitespace(text[cursor_pos - 1]) &&
      cursor_pos == anchor_pos &&
      (cursor_pos == len || base::IsAsciiWhitespace(text[cursor_pos]))) {
    // |text| could be very long, we get at most |kMaxTextBeforeCursorLength|
    // characters before cursor.
    int start_pos = std::max(0, cursor_pos - kMaxTextBeforeCursorLength);
    std::string text_before_cursor =
        text.substr(start_pos, cursor_pos - start_pos);
    std::string suggestion_text = GetPersonalInfoSuggestion(text_before_cursor);
    if (!suggestion_text.empty()) {
      ShowSuggestion(suggestion_text);
      suggestion_shown_ = true;
    }
  }
}

std::string AssistiveSuggester::GetPersonalInfoSuggestion(
    const std::string& text) {
  AssistiveType action = ProposeAssistiveAction(text);
  if (action == AssistiveType::kGenericAction)
    return "";

  if (action == AssistiveType::kPersonalEmail) {
    std::string email = profile_->GetProfileUserName();
    return email;
  } else {
    auto autofill_profiles = personal_data_manager_->GetProfilesToSuggest();
    if (autofill_profiles.size() > 0) {
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
          suggestion = data->GetRawInfo(
              autofill::ServerFieldType::PHONE_HOME_WHOLE_NUMBER);
          break;
        default:
          NOTREACHED();
          break;
      }
      return base::UTF16ToUTF8(suggestion);
    }
  }
  return "";
}

void AssistiveSuggester::ShowSuggestion(const std::string& text) {
  std::string error;
  std::vector<InputMethodEngineBase::SegmentInfo> segments;
  engine_->SetComposition(context_id_, text.c_str(), 0, 0, 0, segments, &error);
  if (!error.empty()) {
    LOG(ERROR) << "Fail to show suggestion. " << error;
  }
}

void AssistiveSuggester::DismissSuggestion() {
  std::string error;
  engine_->ClearComposition(context_id_, &error);
  if (!error.empty()) {
    LOG(ERROR) << "Failed to dismiss suggestion. " << error;
  }
}

}  // namespace chromeos
