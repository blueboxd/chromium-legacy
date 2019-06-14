// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/web_applications/web_app_tab_helper.h"

namespace web_app {

WebAppTabHelper::WebAppTabHelper(content::WebContents* web_contents)
    : WebAppTabHelperBase(web_contents) {}

WebAppTabHelper::~WebAppTabHelper() = default;

// static
WebAppTabHelper* WebAppTabHelper::CreateForWebContents(
    content::WebContents* web_contents) {
  DCHECK(!FromWebContents(web_contents));

  auto tab_helper = std::make_unique<WebAppTabHelper>(web_contents);
  WebAppTabHelper* result = tab_helper.get();
  web_contents->SetUserData(UserDataKey(), std::move(tab_helper));
  return result;
}

WebAppTabHelperBase* WebAppTabHelper::CloneForWebContents(
    content::WebContents* web_contents) const {
  WebAppTabHelper* new_tab_helper =
      WebAppTabHelper::CreateForWebContents(web_contents);
  return new_tab_helper;
}

bool WebAppTabHelper::IsInAppWindow() const {
  // TODO(beccahughes): Implement.
  return false;
}

bool WebAppTabHelper::IsUserInstalled() const {
  // TODO(loyso): Implement it.
  return false;
}

bool WebAppTabHelper::IsFromInstallButton() const {
  // TODO(loyso): Implement it. Plumb |force_shortcut_app| into Registry.
  return true;
}

}  // namespace web_app
