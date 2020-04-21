// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_SETTINGS_ACCESSIBILITY_MAIN_HANDLER_H_
#define CHROME_BROWSER_UI_WEBUI_SETTINGS_ACCESSIBILITY_MAIN_HANDLER_H_

#include "base/macros.h"
#include "chrome/browser/ui/webui/settings/settings_page_ui_handler.h"
#if defined(OS_CHROMEOS)
#include "chrome/browser/chromeos/accessibility/accessibility_manager.h"
#endif  // defined(OS_CHROMEOS)

namespace base {
class ListValue;
}

namespace settings {

// Settings handler for the main accessibility settings page,
// chrome://settings/accessibility.
class AccessibilityMainHandler : public ::settings::SettingsPageUIHandler {
 public:
  AccessibilityMainHandler();
  ~AccessibilityMainHandler() override;

  // SettingsPageUIHandler implementation.
  void RegisterMessages() override;
  void OnJavascriptAllowed() override;
  void OnJavascriptDisallowed() override;

  void HandleGetScreenReaderState(const base::ListValue* args);
  void HandleCheckAccessibilityImageLabels(const base::ListValue* args);

 private:
  void SendScreenReaderStateChanged();

#if defined(OS_CHROMEOS)
  void OnAccessibilityStatusChanged(
      const chromeos::AccessibilityStatusEventDetails& details);

  std::unique_ptr<chromeos::AccessibilityStatusSubscription>
      accessibility_subscription_;
#else

#endif  // defined(OS_CHROMEOS)

  DISALLOW_COPY_AND_ASSIGN(AccessibilityMainHandler);
};

}  // namespace settings

#endif  // CHROME_BROWSER_UI_WEBUI_SETTINGS_ACCESSIBILITY_MAIN_HANDLER_H_
