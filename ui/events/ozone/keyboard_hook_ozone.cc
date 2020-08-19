// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/events/ozone/keyboard_hook_ozone.h"

#include <utility>

#include "base/callback.h"
#include "base/macros.h"
#include "base/optional.h"
#include "build/build_config.h"
#include "ui/events/event.h"
#include "ui/events/keycodes/dom/dom_code.h"
#include "ui/gfx/native_widget_types.h"

namespace ui {

KeyboardHookOzone::KeyboardHookOzone(
    base::Optional<base::flat_set<DomCode>> dom_codes,
    KeyEventCallback callback)
    : KeyboardHookBase(std::move(dom_codes), std::move(callback)) {}

KeyboardHookOzone::~KeyboardHookOzone() = default;

bool KeyboardHookOzone::RegisterHook() {
  // TODO(680809): Implement system-level keyboard lock feature for ozone.
  // Return true to enable browser-level keyboard lock for ozone platform.
  return true;
}

#if !defined(OS_LINUX) && !defined(OS_CHROMEOS)
// static
std::unique_ptr<KeyboardHook> KeyboardHook::CreateModifierKeyboardHook(
    base::Optional<base::flat_set<DomCode>> dom_codes,
    gfx::AcceleratedWidget accelerated_widget,
    KeyEventCallback callback) {
  std::unique_ptr<KeyboardHookOzone> keyboard_hook =
      std::make_unique<KeyboardHookOzone>(std::move(dom_codes),
                                          std::move(callback));

  if (!keyboard_hook->RegisterHook())
    return nullptr;

  return keyboard_hook;
}

// static
std::unique_ptr<KeyboardHook> KeyboardHook::CreateMediaKeyboardHook(
    KeyEventCallback callback) {
  return nullptr;
}
#endif

}  // namespace ui
