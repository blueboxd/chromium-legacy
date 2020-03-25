// Copyright (c) 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// This file implements the input method suggestion window used on Chrome OS.

#ifndef CHROME_BROWSER_CHROMEOS_INPUT_METHOD_SUGGESTION_WINDOW_CONTROLLER_H_
#define CHROME_BROWSER_CHROMEOS_INPUT_METHOD_SUGGESTION_WINDOW_CONTROLLER_H_

namespace chromeos {
namespace input_method {

// SuggestionWindowController is used for controlling the input method
// suggestion window. Once the initialization is done, the controller
// starts monitoring signals sent from the the background input method
// daemon, and shows and hides the suggestion window as needed. Upon
// deletion of the object, monitoring stops and the view used for
// rendering the suggestion view is deleted.
class SuggestionWindowController {
 public:
  virtual ~SuggestionWindowController() = default;

  // Gets an instance of SuggestionWindowController. Caller has to delete the
  // returned object.
  static SuggestionWindowController* CreateSuggestionWindowController();
};

}  // namespace input_method
}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_INPUT_METHOD_SUGGESTION_WINDOW_CONTROLLER_H_
