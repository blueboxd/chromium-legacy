// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/ntp/cookie_controls_handler.h"

#include "base/bind.h"
#include "base/values.h"
#include "chrome/browser/profiles/profile.h"
#include "components/content_settings/core/browser/cookie_settings.h"
#include "components/content_settings/core/common/pref_names.h"
#include "components/prefs/pref_service.h"

CookieControlsHandler::CookieControlsHandler() {}

CookieControlsHandler::~CookieControlsHandler() {}

void CookieControlsHandler::RegisterMessages() {
  web_ui()->RegisterMessageCallback(
      "cookieControlsToggleChanged",
      base::BindRepeating(
          &CookieControlsHandler::HandleCookieControlsToggleChanged,
          base::Unretained(this)));
  web_ui()->RegisterMessageCallback(
      "observeCookieControlsModeChange",
      base::BindRepeating(
          &CookieControlsHandler::HandleObserveCookieControlsModeChange,
          base::Unretained(this)));
}

void CookieControlsHandler::OnJavascriptAllowed() {
  Profile* profile = Profile::FromWebUI(web_ui());
  pref_change_registrar_.Init(profile->GetPrefs());
  pref_change_registrar_.Add(
      prefs::kCookieControlsMode,
      base::Bind(&CookieControlsHandler::OnCookieControlsChanged,
                 base::Unretained(this)));
}

void CookieControlsHandler::OnJavascriptDisallowed() {
  pref_change_registrar_.RemoveAll();
}

void CookieControlsHandler::HandleCookieControlsToggleChanged(
    const base::ListValue* args) {
  bool checked;
  CHECK(args->GetBoolean(0, &checked));
  Profile* profile = Profile::FromWebUI(web_ui());
  profile->GetPrefs()->SetInteger(
      prefs::kCookieControlsMode,
      static_cast<int>(
          checked ? content_settings::CookieControlsMode::kIncognitoOnly
                  : content_settings::CookieControlsMode::kOff));
}

void CookieControlsHandler::HandleObserveCookieControlsModeChange(
    const base::ListValue* args) {
  AllowJavascript();
  OnCookieControlsChanged();
}

void CookieControlsHandler::OnCookieControlsChanged() {
  Profile* profile = Profile::FromWebUI(web_ui());
  int mode = profile->GetPrefs()->GetInteger(prefs::kCookieControlsMode);
  base::Value checked(
      mode == static_cast<int>(content_settings::CookieControlsMode::kOff)
          ? false
          : true);
  FireWebUIListener("cookie-controls-changed", checked);
}
