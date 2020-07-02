// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/events/keyboard_hook_base.h"

#include <memory>

#include "base/callback.h"
#include "base/optional.h"
#include "ui/base/ui_base_features.h"
#include "ui/events/keycodes/dom/dom_code.h"
#include "ui/gfx/native_widget_types.h"

#if defined(USE_X11)
#include "ui/events/x/keyboard_hook_x11.h"
#endif

#if defined(USE_OZONE)
#include "ui/events/ozone/keyboard_hook_ozone.h"
#endif

namespace ui {

// static
std::unique_ptr<KeyboardHook> KeyboardHook::CreateModifierKeyboardHook(
    base::Optional<base::flat_set<DomCode>> dom_codes,
    gfx::AcceleratedWidget accelerated_widget,
    KeyEventCallback callback) {
  std::unique_ptr<KeyboardHookBase> keyboard_hook;
#if defined(USE_OZONE)
  if (features::IsUsingOzonePlatform()) {
    keyboard_hook = std::make_unique<KeyboardHookOzone>(std::move(dom_codes),
                                                        std::move(callback));
  }
#endif
#if defined(USE_X11)
  if (!keyboard_hook) {
    keyboard_hook = std::make_unique<KeyboardHookX11>(
        std::move(dom_codes), accelerated_widget, std::move(callback));
  }
#endif
  DCHECK(keyboard_hook);
  if (!keyboard_hook->RegisterHook())
    return nullptr;

  return keyboard_hook;
}

// static
std::unique_ptr<KeyboardHook> KeyboardHook::CreateMediaKeyboardHook(
    KeyEventCallback callback) {
  return nullptr;
}

}  // namespace ui
