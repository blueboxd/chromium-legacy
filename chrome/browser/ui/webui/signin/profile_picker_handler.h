// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_SIGNIN_PROFILE_PICKER_HANDLER_H_
#define CHROME_BROWSER_UI_WEBUI_SIGNIN_PROFILE_PICKER_HANDLER_H_

#include "base/memory/weak_ptr.h"
#include "base/values.h"
#include "content/public/browser/web_ui_message_handler.h"

// The handler for Javascript messages related to the profile picker main view.
class ProfilePickerHandler : public content::WebUIMessageHandler {
 public:
  ProfilePickerHandler();
  ~ProfilePickerHandler() override;

  // content::WebUIMessageHandler:
  void RegisterMessages() override;

 private:
  void HandleMainViewInitialize(const base::ListValue* args);
  void PushProfilesList();
  base::Value GetProfilesList();

  base::WeakPtrFactory<ProfilePickerHandler> weak_factory_{this};

  DISALLOW_COPY_AND_ASSIGN(ProfilePickerHandler);
};

#endif  // CHROME_BROWSER_UI_WEBUI_SIGNIN_PROFILE_PICKER_HANDLER_H_
