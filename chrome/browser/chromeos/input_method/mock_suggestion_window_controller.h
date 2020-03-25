// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_INPUT_METHOD_MOCK_SUGGESTION_WINDOW_CONTROLLER_H_
#define CHROME_BROWSER_CHROMEOS_INPUT_METHOD_MOCK_SUGGESTION_WINDOW_CONTROLLER_H_

#include "base/macros.h"
#include "chrome/browser/chromeos/input_method/suggestion_window_controller.h"

namespace chromeos {
namespace input_method {

// The mock SuggestionWindowController implementation for testing.
class MockSuggestionWindowController : public SuggestionWindowController {
 public:
  MockSuggestionWindowController();
  ~MockSuggestionWindowController() override;

  DISALLOW_COPY_AND_ASSIGN(MockSuggestionWindowController);
};

}  // namespace input_method
}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_INPUT_METHOD_MOCK_SUGGESTION_WINDOW_CONTROLLER_H_
