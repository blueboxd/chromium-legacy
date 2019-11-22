// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_HISTORY_HISTORY_UI_H_
#define CHROME_BROWSER_UI_WEBUI_HISTORY_HISTORY_UI_H_

#include "base/gtest_prod_util.h"
#include "base/macros.h"
#include "content/public/browser/web_ui_controller.h"

namespace base {
class ListValue;
}

namespace user_prefs {
class PrefRegistrySyncable;
}

class HistoryUI : public content::WebUIController {
 public:
  explicit HistoryUI(content::WebUI* web_ui);
  ~HistoryUI() override;

  static void RegisterProfilePrefs(user_prefs::PrefRegistrySyncable* registry);

 private:
  void UpdateDataSource();

  // Handler for the "menuPromoShown" message from the page. No arguments.
  void HandleMenuPromoShown(const base::ListValue* args);

  DISALLOW_COPY_AND_ASSIGN(HistoryUI);
};

#endif  // CHROME_BROWSER_UI_WEBUI_HISTORY_HISTORY_UI_H_
