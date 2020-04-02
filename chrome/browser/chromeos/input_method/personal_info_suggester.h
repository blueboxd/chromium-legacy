// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_INPUT_METHOD_PERSONAL_INFO_SUGGESTER_H_
#define CHROME_BROWSER_CHROMEOS_INPUT_METHOD_PERSONAL_INFO_SUGGESTER_H_

#include <string>

#include "chrome/browser/chromeos/input_method/input_method_engine.h"
#include "chrome/browser/chromeos/input_method/suggester.h"
#include "chrome/browser/chromeos/input_method/suggestion_enums.h"
#include "chrome/browser/ui/input_method/input_method_engine_base.h"

namespace autofill {
class PersonalDataManager;
}  // namespace autofill

class Profile;

namespace chromeos {

AssistiveType ProposeAssistiveAction(const base::string16& text);

// An agent to suggest personal information when the user types, and adopt or
// dismiss the suggestion according to the user action.
class PersonalInfoSuggester : public Suggester {
 public:
  explicit PersonalInfoSuggester(InputMethodEngine* engine, Profile* profile);
  ~PersonalInfoSuggester() override;

  // Suggester overrides:
  void OnFocus(int context_id) override;
  void OnBlur() override;
  SuggestionStatus HandleKeyEvent(
      const ::input_method::InputMethodEngineBase::KeyboardEvent& event)
      override;
  bool Suggest(const base::string16& text) override;
  void DismissSuggestion() override;
  AssistiveType GetProposeActionType() override;

 private:
  // Get the suggestion according to |text_before_cursor|.
  base::string16 GetPersonalInfoSuggestion(
      const base::string16& text_before_cursor);

  void ShowSuggestion(const base::string16& text);

  void AcceptSuggestion();

  InputMethodEngine* const engine_;

  // ID of the focused text field, 0 if none is focused.
  int context_id_ = -1;

  // Assistive type of the last proposed assistive action.
  AssistiveType proposed_action_type_ = AssistiveType::kGenericAction;

  // User's Chrome user profile.
  Profile* const profile_;

  // Personal data manager provided by autofill service.
  autofill::PersonalDataManager* const personal_data_manager_;

  // If we are showing a suggestion right now.
  bool suggestion_shown_ = false;
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_INPUT_METHOD_PERSONAL_INFO_SUGGESTER_H_
