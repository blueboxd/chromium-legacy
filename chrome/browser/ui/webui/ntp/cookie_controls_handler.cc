// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/ntp/cookie_controls_handler.h"

#include <utility>

#include "base/bind.h"
#include "base/feature_list.h"
#include "base/values.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/cookie_controls/cookie_controls_service.h"
#include "chrome/browser/ui/cookie_controls/cookie_controls_service_factory.h"
#include "chrome/common/chrome_features.h"

namespace {
static const char* kPolicyIcon = "cr20:domain";
static const char* kExtensionIcon = "cr:extension";
static const char* kSettingsIcon = "cr:settings_icon";
}  // namespace

CookieControlsHandler::CookieControlsHandler(Profile* profile)
    : service_(CookieControlsServiceFactory::GetForProfile(profile)) {}

CookieControlsHandler::~CookieControlsHandler() {
  service_->RemoveObserver(this);
}

void CookieControlsHandler::RegisterMessages() {
  web_ui()->RegisterMessageCallback(
      "cookieControlsToggleChanged",
      base::BindRepeating(
          &CookieControlsHandler::HandleCookieControlsToggleChanged,
          base::Unretained(this)));
  web_ui()->RegisterMessageCallback(
      "observeCookieControlsSettingsChanges",
      base::BindRepeating(
          &CookieControlsHandler::HandleObserveCookieControlsSettingsChanges,
          base::Unretained(this)));
}

void CookieControlsHandler::OnJavascriptAllowed() {
  service_->AddObserver(this);
}

void CookieControlsHandler::OnJavascriptDisallowed() {
  service_->RemoveObserver(this);
}

void CookieControlsHandler::HandleCookieControlsToggleChanged(
    const base::ListValue* args) {
  bool checked;
  CHECK(args->GetBoolean(0, &checked));
  service_->HandleCookieControlsToggleChanged(checked);
}

void CookieControlsHandler::HandleObserveCookieControlsSettingsChanges(
    const base::ListValue* args) {
  AllowJavascript();
  SendCookieControlsUIChanges();
}

const char* CookieControlsHandler::GetEnforcementIcon(Profile* profile) {
  CookieControlsService* service =
      CookieControlsServiceFactory::GetForProfile(profile);
  switch (service->GetCookieControlsEnforcement()) {
    case CookieControlsEnforcement::kEnforcedByPolicy:
      return kPolicyIcon;
    case CookieControlsEnforcement::kEnforcedByExtension:
      return kExtensionIcon;
    case CookieControlsEnforcement::kEnforcedByCookieSetting:
      return kSettingsIcon;
    case CookieControlsEnforcement::kNoEnforcement:
      return "";
  }
}

void CookieControlsHandler::OnThirdPartyCookieBlockingPrefChanged() {
  SendCookieControlsUIChanges();
}

void CookieControlsHandler::OnThirdPartyCookieBlockingPolicyChanged() {
  SendCookieControlsUIChanges();
}

void CookieControlsHandler::SendCookieControlsUIChanges() {
  Profile* profile = Profile::FromWebUI(web_ui());
  base::DictionaryValue dict;
  dict.SetBoolKey("enforced", service_->ShouldEnforceCookieControls());
  dict.SetBoolKey("checked", service_->GetToggleCheckedValue());
  dict.SetStringKey("icon", CookieControlsHandler::GetEnforcementIcon(profile));
  bool use_new_cookie_page =
      base::FeatureList::IsEnabled(features::kPrivacySettingsRedesign);
  dict.SetString("cookieSettingsUrl",
                 use_new_cookie_page ? "chrome://settings/cookies"
                                     : "chrome://settings/content/cookies");
  FireWebUIListener("cookie-controls-changed", dict);
}
