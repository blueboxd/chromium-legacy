// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_BASE_POINTER_TOUCH_UI_CONTROLLER_H_
#define UI_BASE_POINTER_TOUCH_UI_CONTROLLER_H_

#include <string>

#include "base/callback_list.h"
#include "build/build_config.h"
#include "ui/base/ui_base_export.h"

#if defined(OS_WIN)
namespace gfx {
class SingletonHwndObserver;
}
#endif

namespace ui {

// Central controller to handle touch UI modes.
class UI_BASE_EXPORT TouchUiController {
 public:
  using CallbackList = base::CallbackList<void()>;
  using Subscription = CallbackList::Subscription;

  enum class TouchUiState {
    kDisabled,
    kAuto,
    kEnabled,
  };

  class UI_BASE_EXPORT TouchUiScoperForTesting {
   public:
    explicit TouchUiScoperForTesting(bool enabled,
                                     TouchUiController* controller = Get());
    TouchUiScoperForTesting(const TouchUiScoperForTesting&) = delete;
    TouchUiScoperForTesting& operator=(const TouchUiScoperForTesting&) = delete;
    ~TouchUiScoperForTesting();

   private:
    TouchUiController* const controller_;
    const TouchUiState old_state_;
  };

  static TouchUiController* Get();

  explicit TouchUiController(TouchUiState touch_ui_state = TouchUiState::kAuto);
  TouchUiController(const TouchUiController&) = delete;
  TouchUiController& operator=(const TouchUiController&) = delete;
  ~TouchUiController();

  bool touch_ui() const {
    return (touch_ui_state_ == TouchUiState::kEnabled) ||
           ((touch_ui_state_ == TouchUiState::kAuto) && tablet_mode_);
  }

  std::unique_ptr<Subscription> RegisterCallback(
      const base::RepeatingClosure& closure);

  void OnTabletModeToggled(bool enabled);

 private:
  TouchUiState SetTouchUiState(TouchUiState touch_ui_state);

  bool tablet_mode_ = false;
  TouchUiState touch_ui_state_;

#if defined(OS_WIN)
  std::unique_ptr<gfx::SingletonHwndObserver> singleton_hwnd_observer_;
#endif

  CallbackList callback_list_;
};

}  // namespace ui

#endif  // UI_BASE_POINTER_TOUCH_UI_CONTROLLER_H_
