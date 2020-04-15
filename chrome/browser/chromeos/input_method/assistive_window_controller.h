// Copyright (c) 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// This file implements the assistive window used on Chrome OS.

#ifndef CHROME_BROWSER_CHROMEOS_INPUT_METHOD_ASSISTIVE_WINDOW_CONTROLLER_H_
#define CHROME_BROWSER_CHROMEOS_INPUT_METHOD_ASSISTIVE_WINDOW_CONTROLLER_H_

namespace chromeos {
namespace input_method {

// AssistiveWindowController is used for controlling the assistive window.
// Once the initialization is done, the controller starts monitoring signals
// sent from the the background input method daemon, and shows and hides the
// assistive window as needed. Upon deletion of the object, monitoring stops
// and the view used for rendering the assistive view is deleted.
class AssistiveWindowController {
 public:
  virtual ~AssistiveWindowController() = default;

  // Gets an instance of AssistiveWindowController. Caller has to delete the
  // returned object.
  static AssistiveWindowController* CreateAssistiveWindowController();
};

}  // namespace input_method
}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_INPUT_METHOD_ASSISTIVE_WINDOW_CONTROLLER_H_
