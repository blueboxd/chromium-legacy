// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_SETTINGS_SAFE_BROWSING_HANDLER_H_
#define CHROME_BROWSER_UI_WEBUI_SETTINGS_SAFE_BROWSING_HANDLER_H_

#include "chrome/browser/ui/webui/settings/settings_page_ui_handler.h"
#include "chrome/browser/ui/webui/settings/site_settings_helper.h"

#include "chrome/browser/profiles/profile.h"

namespace settings {

constexpr char kSafeBrowsingEnhanced[] = "enhanced";
constexpr char kSafeBrowsingStandard[] = "standard";
constexpr char kSafeBrowsingDisabled[] = "disabled";

struct SafeBrowsingRadioManagedState {
  site_settings::ManagedState enhanced;
  site_settings::ManagedState standard;
  site_settings::ManagedState disabled;
};

// Settings page UI handler that provides representation of Safe Browsing
// settings.
class SafeBrowsingHandler : public SettingsPageUIHandler {
 public:
  explicit SafeBrowsingHandler(Profile* profile);
  ~SafeBrowsingHandler() override;

  // WebUIMessageHandler implementation.
  void RegisterMessages() override;

  // Calculate and return the current Safe Browsing radio buttons.
  void HandleGetSafeBrowsingRadioManagedState(const base::ListValue* args);

  // Confirm that the current Safe Browsing Enhanced preference is appropriate
  // for the currently enabled features, updating it if required.
  void HandleValidateSafeBrowsingEnhanced(const base::ListValue* args);

 private:
  friend class SafeBrowsingHandlerTest;
  FRIEND_TEST_ALL_PREFIXES(SafeBrowsingHandlerTest, GenerateRadioManagedState);
  FRIEND_TEST_ALL_PREFIXES(SafeBrowsingHandlerTest, ProvideRadioManagedState);

  // SettingsPageUIHandler implementation.
  void OnJavascriptAllowed() override {}
  void OnJavascriptDisallowed() override {}

  // Calculate the current Safe Browsing control state for the provided profile.
  static SafeBrowsingRadioManagedState GetSafeBrowsingRadioManagedState(
      Profile* profile);

  Profile* profile_;

  DISALLOW_COPY_AND_ASSIGN(SafeBrowsingHandler);
};

}  // namespace settings

#endif  // CHROME_BROWSER_UI_WEBUI_SETTINGS_SAFE_BROWSING_HANDLER_H_
