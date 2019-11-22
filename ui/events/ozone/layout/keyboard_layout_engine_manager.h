// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_OZONE_PUBLIC_KEYBOARD_LAYOUT_ENGINE_MANAGER_H_
#define UI_OZONE_PUBLIC_KEYBOARD_LAYOUT_ENGINE_MANAGER_H_

#include <memory>

#include "base/component_export.h"
#include "base/macros.h"

namespace ui {

class KeyboardLayoutEngine;

class COMPONENT_EXPORT(EVENTS_OZONE_LAYOUT) KeyboardLayoutEngineManager {
 public:
  virtual ~KeyboardLayoutEngineManager();

  static void SetKeyboardLayoutEngine(
      std::unique_ptr<KeyboardLayoutEngine> engine);
  static KeyboardLayoutEngine* GetKeyboardLayoutEngine();

 private:
  KeyboardLayoutEngineManager(KeyboardLayoutEngine* engine);

  static KeyboardLayoutEngineManager* instance_;
  std::unique_ptr<KeyboardLayoutEngine> keyboard_layout_engine_;

  DISALLOW_COPY_AND_ASSIGN(KeyboardLayoutEngineManager);
};

}  // namespace ui

#endif  // UI_OZONE_PUBLIC_KEYBOARD_LAYOUT_ENGINE_MANAGER_H_
