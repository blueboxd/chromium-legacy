// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/signin/dice_web_signin_intercept_ui.h"

#include "chrome/browser/profiles/profile.h"
#include "chrome/common/webui_url_constants.h"
#include "chrome/grit/browser_resources.h"
#include "content/public/browser/web_ui_data_source.h"

DiceWebSigninInterceptUI::DiceWebSigninInterceptUI(content::WebUI* web_ui)
    : content::WebUIController(web_ui) {
  content::WebUIDataSource* html_source = content::WebUIDataSource::Create(
      chrome::kChromeUIDiceWebSigninInterceptHost);
  html_source->SetDefaultResource(IDR_SIGNIN_DICE_WEB_INTERCEPT_HTML);
  content::WebUIDataSource::Add(Profile::FromWebUI(web_ui), html_source);
}

DiceWebSigninInterceptUI::~DiceWebSigninInterceptUI() = default;
