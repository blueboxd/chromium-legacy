// Copyright (c) 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_INPUT_METHOD_MOCK_ASSISTIVE_WINDOW_CONTROLLER_H_
#define CHROME_BROWSER_CHROMEOS_INPUT_METHOD_MOCK_ASSISTIVE_WINDOW_CONTROLLER_H_

#include "base/macros.h"
#include "chrome/browser/chromeos/input_method/assistive_window_controller.h"

namespace chromeos {
namespace input_method {

// The mock AssistiveWindowController implementation for testing.
class MockAssistiveWindowController : public AssistiveWindowController {
 public:
  MockAssistiveWindowController();
  ~MockAssistiveWindowController() override;

  DISALLOW_COPY_AND_ASSIGN(MockAssistiveWindowController);
};

}  // namespace input_method
}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_INPUT_METHOD_MOCK_ASSISTIVE_WINDOW_CONTROLLER_H_
