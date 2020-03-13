// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_CHROMEOS_LOGIN_GESTURE_NAVIGATION_SCREEN_HANDLER_H_
#define CHROME_BROWSER_UI_WEBUI_CHROMEOS_LOGIN_GESTURE_NAVIGATION_SCREEN_HANDLER_H_

#include "chrome/browser/ui/webui/chromeos/login/base_screen_handler.h"

namespace chromeos {

class GestureNavigationScreen;

// Interface between gesture navigation screen and its representation.
class GestureNavigationScreenView {
 public:
  constexpr static StaticOobeScreenId kScreenId{"gesture-navigation"};

  virtual ~GestureNavigationScreenView() {}

  virtual void Bind(GestureNavigationScreen* screen) = 0;
  virtual void Show() = 0;
  virtual void Hide() = 0;
};

// WebUI implementation of GestureNavigationScreenView.
class GestureNavigationScreenHandler : public GestureNavigationScreenView,
                                       public BaseScreenHandler {
 public:
  using TView = GestureNavigationScreenView;

  explicit GestureNavigationScreenHandler(JSCallsContainer* js_calls_container);
  ~GestureNavigationScreenHandler() override;

  GestureNavigationScreenHandler(const GestureNavigationScreenHandler&) =
      delete;
  GestureNavigationScreenHandler operator=(
      const GestureNavigationScreenHandler&) = delete;

  // GestureNavigationScreenView:
  void Bind(GestureNavigationScreen* screen) override;
  void Show() override;
  void Hide() override;

  // BaseScreenHandler:
  void DeclareLocalizedValues(
      ::login::LocalizedValuesBuilder* builder) override;
  void Initialize() override;
  void RegisterMessages() override;

 private:
  // Called when the currently shown page for the gesture navigation screen is
  // changed.
  void HandleGesturePageChange(const std::string& new_page);

  GestureNavigationScreen* screen_ = nullptr;

  // If true, Initialize() will call Show().
  bool show_on_init_ = false;
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_UI_WEBUI_CHROMEOS_LOGIN_GESTURE_NAVIGATION_SCREEN_HANDLER_H_
