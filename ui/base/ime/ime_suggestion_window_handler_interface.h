// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_BASE_IME_IME_SUGGESTION_WINDOW_HANDLER_INTERFACE_H_
#define UI_BASE_IME_IME_SUGGESTION_WINDOW_HANDLER_INTERFACE_H_

#include <stdint.h>

#include "base/component_export.h"
#include "base/strings/string16.h"

namespace gfx {
class Rect;
}  // namespace gfx

namespace chromeos {

// A interface to handle the suggestion window related method call.
class COMPONENT_EXPORT(UI_BASE_IME) IMESuggestionWindowHandlerInterface {
 public:
  virtual ~IMESuggestionWindowHandlerInterface() {}

  // Called when showing/hiding suggestion window.
  virtual void Show(const base::string16& text) {}
  virtual void Hide() {}

  // Called to get the current suggestion text.
  virtual base::string16 GetText() const = 0;

  // Called when the application changes its caret bounds.
  virtual void SetBounds(const gfx::Rect& cursor_bounds) = 0;

  // Called when the text field's focus state is changed.
  virtual void FocusStateChanged() {}

 protected:
  IMESuggestionWindowHandlerInterface() {}
};

}  // namespace chromeos

#endif  // UI_BASE_IME_IME_SUGGESTION_WINDOW_HANDLER_INTERFACE_H_
