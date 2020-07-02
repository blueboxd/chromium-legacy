// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_EVENTS_OZONE_KEYBOARD_HOOK_OZONE_H_
#define UI_EVENTS_OZONE_KEYBOARD_HOOK_OZONE_H_

#include "base/callback.h"
#include "base/optional.h"
#include "ui/events/keyboard_hook_base.h"

namespace ui {

// A default implementation for Ozone platform.
class KeyboardHookOzone : public KeyboardHookBase {
 public:
  KeyboardHookOzone(base::Optional<base::flat_set<DomCode>> dom_codes,
                    KeyEventCallback callback);
  KeyboardHookOzone(const KeyboardHookOzone&) = delete;
  KeyboardHookOzone& operator=(const KeyboardHookOzone&) = delete;
  ~KeyboardHookOzone() override;

  // KeyboardHookBase:
  bool RegisterHook() override;
};

}  // namespace ui

#endif  // UI_EVENTS_OZONE_KEYBOARD_HOOK_OZONE_H_
