// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_CHROMEOS_IME_ASSISTIVE_DELEGATE_H_
#define UI_CHROMEOS_IME_ASSISTIVE_DELEGATE_H_

#include "ui/chromeos/ui_chromeos_export.h"

namespace ui {
namespace ime {

enum class ButtonId {
  kUndo,
  kAddToDictionary,
};

enum class AssistiveWindowType {
  kUndoWindow,
};

class UI_CHROMEOS_EXPORT AssistiveDelegate {
 public:
  // Invoked when a button in an assistive window is clicked.
  virtual void AssistiveWindowClicked(ButtonId id,
                                      AssistiveWindowType type) = 0;

 protected:
  virtual ~AssistiveDelegate() = default;
};

}  // namespace ime
}  // namespace ui

#endif  //  UI_CHROMEOS_IME_ASSISTIVE_DELEGATE_H_
