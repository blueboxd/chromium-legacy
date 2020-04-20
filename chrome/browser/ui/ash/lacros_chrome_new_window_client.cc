// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/ash/lacros_chrome_new_window_client.h"

#include <string>
#include <vector>

#include "chromeos/dbus/upstart/upstart_client.h"
#include "url/gurl.h"

LacrosChromeNewWindowClient::LacrosChromeNewWindowClient() = default;

LacrosChromeNewWindowClient::~LacrosChromeNewWindowClient() = default;

void LacrosChromeNewWindowClient::NewTab() {
  chromeos::UpstartClient::Get()->StartLacrosChrome({});
}

void LacrosChromeNewWindowClient::NewTabWithUrl(const GURL& url,
                                                bool from_user_interaction) {
  std::string env_url = "URL=" + url.spec();
  chromeos::UpstartClient::Get()->StartLacrosChrome({env_url});
}

void LacrosChromeNewWindowClient::NewWindow(bool incognito) {
  std::vector<std::string> env = {"NEW_WINDOW=1"};
  if (incognito)
    env.push_back("INCOGNITO=1");

  chromeos::UpstartClient::Get()->StartLacrosChrome(env);
}
