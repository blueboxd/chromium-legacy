// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_ASH_LACROS_CHROME_NEW_WINDOW_CLIENT_H_
#define CHROME_BROWSER_UI_ASH_LACROS_CHROME_NEW_WINDOW_CLIENT_H_

#include "chrome/browser/ui/ash/chrome_new_window_client.h"

// Handles opening new tabs and windows which is connected to lacros-chrome
// for experiment.
class LacrosChromeNewWindowClient : public ChromeNewWindowClient {
 public:
  LacrosChromeNewWindowClient();
  ~LacrosChromeNewWindowClient() override;

  // Overridden from ash::NewWindowDelegate:
  void NewTab() override;
  void NewTabWithUrl(const GURL& url, bool from_user_interaction) override;
  void NewWindow(bool incognito) override;
};

#endif  // CHROME_BROWSER_UI_ASH_LACROS_CHROME_NEW_WINDOW_CLIENT_H_
