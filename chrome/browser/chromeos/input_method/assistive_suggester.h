// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_INPUT_METHOD_ASSISTIVE_SUGGESTER_H_
#define CHROME_BROWSER_CHROMEOS_INPUT_METHOD_ASSISTIVE_SUGGESTER_H_

#include <string>

#include "chrome/browser/chromeos/input_method/input_method_engine.h"
#include "chrome/browser/ui/input_method/input_method_engine_base.h"

namespace autofill {
class PersonalDataManager;
}  // namespace autofill

class Profile;

namespace chromeos {

// An agent to suggest assistive information when the user types, and adopt or
// dismiss the suggestion according to the user action.
class AssistiveSuggester {
 public:
  AssistiveSuggester(InputMethodEngine* engine, Profile* profile);

  // Called when a text field gains focus, and suggester starts working.
  void OnFocus(int context_id);

  // Called when a text field loses focus, and suggester stops working.
  void OnBlur();

  // Checks the text before cursor, emits metric if any assistive prefix is
  // matched.
  void RecordAssitiveCoverageMetrics(const std::string& text,
                                     int cursor_pos,
                                     int anchor_pos);

  // Called when a surrounding text is changed.
  // Returns true if it changes the surrounding text, e.g. a suggestion is
  // generated or dismissed.
  bool OnSurroundingTextChanged(const std::string& text,
                                int cursor_pos,
                                int anchor_pos);

  // Called when the user pressed a key.
  // Returns true if suggester handles the event and it should stop propagate.
  bool OnKeyEvent(
      const ::input_method::InputMethodEngineBase::KeyboardEvent& event);

 private:
  // Get the suggestion according to |text_before_cursor|.
  std::string GetPersonalInfoSuggestion(const std::string& text_before_cursor);

  // Check if any suggestion text should be displayed according to the
  // surrounding text information.
  void Suggest(const std::string& text, int cursor_pos, int anchor_pos);

  void ShowSuggestion(const std::string& text);
  void DismissSuggestion();

  InputMethodEngine* engine_;

  // ID of the focused text field, 0 if none is focused.
  int context_id_ = -1;

  // User's Chrome user profile.
  Profile* profile_;

  // Personal data manager provided by autofill service.
  autofill::PersonalDataManager* personal_data_manager_;

  // If we are showing a suggestion right now.
  bool suggestion_shown_ = false;

  // If the suggestion is dismissed by the user, this is necessary so that we
  // will not reshow the suggestion immediately after the user dismisses it.
  bool suggestion_dismissed_ = false;
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_INPUT_METHOD_ASSISTIVE_SUGGESTER_H_
