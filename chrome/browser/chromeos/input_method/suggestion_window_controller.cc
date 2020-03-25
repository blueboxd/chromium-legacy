// Copyright (c) 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/input_method/suggestion_window_controller.h"

#include "chrome/browser/chromeos/input_method/suggestion_window_controller_impl.h"

namespace chromeos {
namespace input_method {

// static
SuggestionWindowController*
SuggestionWindowController::CreateSuggestionWindowController() {
  return new SuggestionWindowControllerImpl;
}

}  // namespace input_method
}  // namespace chromeos
