// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_INPUT_METHOD_SUGGESTION_HANDLER_INTERFACE_H_
#define CHROME_BROWSER_CHROMEOS_INPUT_METHOD_SUGGESTION_HANDLER_INTERFACE_H_

#include <string>
#include "base/strings/string16.h"

namespace chromeos {

// An interface to handler suggestion related calls from assistive suggester.
class SuggestionHandlerInterface {
 public:
  virtual ~SuggestionHandlerInterface() = default;

  // Dismiss suggestion window.
  virtual bool DismissSuggestion(int context_id, std::string* error) = 0;

  // Sets text and show suggestion window.
  // text - the full suggestion text.
  // confirmed_text - the confirmed text that the user has typed so far.
  // show_tab - whether to show "tab" in the suggestion window.
  virtual bool SetSuggestion(int context_id,
                             const base::string16& text,
                             const size_t confirmed_length,
                             const bool show_tab,
                             std::string* error) = 0;

  // Commit the suggestion and hide the window.
  virtual bool AcceptSuggestion(int context_id, std::string* error) = 0;
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_INPUT_METHOD_SUGGESTION_HANDLER_INTERFACE_H_
