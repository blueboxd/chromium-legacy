// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_INPUT_METHOD_SUGGESTION_ENUMS_H_
#define CHROME_BROWSER_CHROMEOS_INPUT_METHOD_SUGGESTION_ENUMS_H_

#include <string>

#include "chrome/browser/chromeos/input_method/input_method_engine.h"
#include "chrome/browser/ui/input_method/input_method_engine_base.h"

namespace chromeos {

// Must match with IMEAssistiveAction in enums.xml
enum class AssistiveType {
  kGenericAction = 0,
  kPersonalEmail = 1,
  kPersonalAddress = 2,
  kPersonalPhoneNumber = 3,
  kPersonalName = 4,
  kEmoji = 5,
  kAssistiveAutocorrect = 6,
  kMaxValue = kAssistiveAutocorrect,
};

enum class SuggestionStatus {
  kNotHandled = 0,
  kAccept = 1,
  kDismiss = 2,
  kBrowsing = 3,
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_INPUT_METHOD_SUGGESTION_ENUMS_H_
