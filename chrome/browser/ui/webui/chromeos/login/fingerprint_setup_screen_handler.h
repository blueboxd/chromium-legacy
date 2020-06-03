// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_CHROMEOS_LOGIN_FINGERPRINT_SETUP_SCREEN_HANDLER_H_
#define CHROME_BROWSER_UI_WEBUI_CHROMEOS_LOGIN_FINGERPRINT_SETUP_SCREEN_HANDLER_H_

#include "base/macros.h"
#include "chrome/browser/ui/webui/chromeos/login/base_screen_handler.h"
#include "services/device/public/mojom/fingerprint.mojom.h"

namespace chromeos {

class FingerprintSetupScreen;

// Interface for dependency injection between FingerprintSetupScreen and its
// WebUI representation.
class FingerprintSetupScreenView {
 public:
  constexpr static StaticOobeScreenId kScreenId{"fingerprint-setup"};

  virtual ~FingerprintSetupScreenView() = default;

  // Sets screen this view belongs to.
  virtual void Bind(FingerprintSetupScreen* screen) = 0;

  // Shows the contents of the screen.
  virtual void Show() = 0;

  // Hides the contents of the screen.
  virtual void Hide() = 0;

  // Enables adding new finger.
  virtual void EnableAddAnotherFinger(bool enable) = 0;

  // Trigger update UI state due to enroll status update.
  virtual void OnEnrollScanDone(device::mojom::ScanResult scan_result,
                                bool enroll_session_complete,
                                int percent_complete) = 0;
};

// The sole implementation of the FingerprintSetupScreenView, using WebUI.
class FingerprintSetupScreenHandler : public BaseScreenHandler,
                                      public FingerprintSetupScreenView {
 public:
  using TView = FingerprintSetupScreenView;

  explicit FingerprintSetupScreenHandler(JSCallsContainer* js_calls_container);
  ~FingerprintSetupScreenHandler() override;

  // BaseScreenHandler:
  void DeclareLocalizedValues(
      ::login::LocalizedValuesBuilder* builder) override;
  void RegisterMessages() override;

  // FingerprintSetupScreenView:
  void Bind(FingerprintSetupScreen* screen) override;
  void Show() override;
  void Hide() override;
  void EnableAddAnotherFinger(bool enable) override;
  void OnEnrollScanDone(device::mojom::ScanResult scan_result,
                        bool enroll_session_complete,
                        int percent_complete) override;

  // BaseScreenHandler:
  void Initialize() override;

 private:
  FingerprintSetupScreen* screen_ = nullptr;

  DISALLOW_COPY_AND_ASSIGN(FingerprintSetupScreenHandler);
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_UI_WEBUI_CHROMEOS_LOGIN_FINGERPRINT_SETUP_SCREEN_HANDLER_H_
