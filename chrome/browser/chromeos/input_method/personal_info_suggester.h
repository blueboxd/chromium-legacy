// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_INPUT_METHOD_PERSONAL_INFO_SUGGESTER_H_
#define CHROME_BROWSER_CHROMEOS_INPUT_METHOD_PERSONAL_INFO_SUGGESTER_H_

#include <string>

#include "chrome/browser/chromeos/input_method/suggester.h"
#include "chrome/browser/chromeos/input_method/suggestion_enums.h"
#include "chrome/browser/chromeos/input_method/suggestion_handler_interface.h"
#include "chrome/browser/ui/input_method/input_method_engine_base.h"
#include "content/public/browser/tts_controller.h"

namespace autofill {
class PersonalDataManager;
}  // namespace autofill

class Profile;

namespace chromeos {

AssistiveType ProposeAssistiveAction(const base::string16& text);

class TtsHandler : public content::UtteranceEventDelegate {
 public:
  explicit TtsHandler(Profile* profile);
  ~TtsHandler() override;

  // Announce |text| after some |delay|. The delay is to avoid conflict with
  // other ChromeVox announcements. This should be no-op if ChromeVox is not
  // enabled.
  void Announce(const std::string& text,
                const base::TimeDelta delay = base::TimeDelta());

  // UtteranceEventDelegate implementation.
  void OnTtsEvent(content::TtsUtterance* utterance,
                  content::TtsEventType event_type,
                  int char_index,
                  int length,
                  const std::string& error_message) override;

 private:
  virtual void Speak(const std::string& text);

  Profile* const profile_;
  std::unique_ptr<base::OneShotTimer> delay_timer_;
};

// An agent to suggest personal information when the user types, and adopt or
// dismiss the suggestion according to the user action.
class PersonalInfoSuggester : public Suggester {
 public:
  // If |personal_data_manager| is nullptr, we will obtain it from
  // |PersonalDataManagerFactory| according to |profile|.
  PersonalInfoSuggester(
      SuggestionHandlerInterface* suggestion_handler,
      Profile* profile,
      autofill::PersonalDataManager* personal_data_manager = nullptr,
      std::unique_ptr<TtsHandler> tts_handler = nullptr);
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
  // Get the suggestion according to |text|.
  base::string16 GetSuggestion(const base::string16& text);

  void ShowSuggestion(const base::string16& text,
                      const size_t confirmed_length);

  void AcceptSuggestion();

  SuggestionHandlerInterface* const suggestion_handler_;

  // ID of the focused text field, 0 if none is focused.
  int context_id_ = -1;

  // Assistive type of the last proposed assistive action.
  AssistiveType proposed_action_type_ = AssistiveType::kGenericAction;

  // User's Chrome user profile.
  Profile* const profile_;

  // Personal data manager provided by autofill service.
  autofill::PersonalDataManager* const personal_data_manager_;

  // The handler to handle Text-to-Speech (TTS) request.
  std::unique_ptr<TtsHandler> const tts_handler_;

  // If we are showing a suggestion right now.
  bool suggestion_shown_ = false;

  // The current suggestion text shown.
  base::string16 suggestion_;
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_INPUT_METHOD_PERSONAL_INFO_SUGGESTER_H_
