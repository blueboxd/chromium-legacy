// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_CONTENT_SETTINGS_IOS_COOKIE_BLOCKER_H_
#define IOS_CHROME_BROWSER_CONTENT_SETTINGS_IOS_COOKIE_BLOCKER_H_

#import <WebKit/WebKit.h>

#include "base/gtest_prod_util.h"
#include "base/memory/weak_ptr.h"
#include "components/content_settings/core/browser/cookie_settings.h"
#include "components/keyed_service/core/keyed_service.h"

class ChromeBrowserState;
namespace web {
enum class CookieBlockingMode;
}

// Class for translating Chrome-wide cookie settings into //ios/web cookie
// blocking updates.
class IOSCookieBlocker : public content_settings::CookieSettings::Observer,
                         public KeyedService {
 public:
  IOSCookieBlocker(
      ChromeBrowserState* browser_state,
      scoped_refptr<content_settings::CookieSettings> cookie_settings);
  IOSCookieBlocker(const IOSCookieBlocker&) = delete;
  IOSCookieBlocker& operator=(const IOSCookieBlocker&) = delete;

  ~IOSCookieBlocker() override;

 private:
  // Gets the current cookie blocking mode based on the
  // data stored on the attached |CookieSettings|.
  web::CookieBlockingMode GetCurrentCookieBlockingMode();
  // Updates the cookie blocking mode of the attached |BrowserState|
  // to the correct mode from |CookieSettings|.
  void UpdateBrowserStateCookieBlockingMode();

  // content_settings::CookieSettings::Observer
  void OnThirdPartyCookieBlockingChanged(
      bool block_third_party_cookies) override;
  void OnCookieSettingChanged() override;

  // KeyedService
  void Shutdown() override;

  ChromeBrowserState* browser_state_ = nullptr;
  scoped_refptr<content_settings::CookieSettings> cookie_settings_ = nullptr;
};

#endif  // IOS_CHROME_BROWSER_CONTENT_SETTINGS_IOS_COOKIE_BLOCKER_H_
