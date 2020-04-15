// Copyright (c) 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/input_method/assistive_window_controller.h"

#include "chrome/browser/chromeos/input_method/assistive_window_controller_impl.h"

namespace chromeos {
namespace input_method {

// static
AssistiveWindowController*
AssistiveWindowController::CreateAssistiveWindowController() {
  return new AssistiveWindowControllerImpl;
}

}  // namespace input_method
}  // namespace chromeos
