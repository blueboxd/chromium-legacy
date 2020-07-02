// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_INPUT_METHOD_UI_ASSISTIVE_DELEGATE_H_
#define CHROME_BROWSER_CHROMEOS_INPUT_METHOD_UI_ASSISTIVE_DELEGATE_H_

#include "base/strings/string16.h"
#include "ui/chromeos/ui_chromeos_export.h"

namespace ui {
namespace ime {

enum class ButtonId {
  kNone,
  kUndo,
  kAddToDictionary,
  kSmartInputsSettingLink,
  kSuggestion,
};

enum class AssistiveWindowType {
  kNone,
  kUndoWindow,
  kEmojiSuggestion,
};

struct AssistiveWindowButton {
  ButtonId id = ButtonId::kNone;
  AssistiveWindowType window_type = AssistiveWindowType::kNone;
  size_t index = -1;
  std::string announce_string;
};

class UI_CHROMEOS_EXPORT AssistiveDelegate {
 public:
  // Invoked when a button in an assistive window is clicked.
  virtual void AssistiveWindowButtonClicked(
      const AssistiveWindowButton& button) const = 0;

 protected:
  virtual ~AssistiveDelegate() = default;
};

}  // namespace ime
}  // namespace ui

#endif  //  CHROME_BROWSER_CHROMEOS_INPUT_METHOD_UI_ASSISTIVE_DELEGATE_H_
