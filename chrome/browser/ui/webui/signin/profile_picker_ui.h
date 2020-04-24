// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_SIGNIN_PROFILE_PICKER_UI_H_
#define CHROME_BROWSER_UI_WEBUI_SIGNIN_PROFILE_PICKER_UI_H_

#include "content/public/browser/web_ui_controller.h"

// The WebUI controller for chrome://profile-picker/.
class ProfilePickerUI : public content::WebUIController {
 public:
  explicit ProfilePickerUI(content::WebUI* web_ui);
  ~ProfilePickerUI() override;

 private:
  DISALLOW_COPY_AND_ASSIGN(ProfilePickerUI);
};

#endif  // CHROME_BROWSER_UI_WEBUI_SIGNIN_PROFILE_PICKER_UI_H_
