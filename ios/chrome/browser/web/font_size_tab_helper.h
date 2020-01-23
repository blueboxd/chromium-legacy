// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_WEB_FONT_SIZE_TAB_HELPER_H_
#define IOS_CHROME_BROWSER_WEB_FONT_SIZE_TAB_HELPER_H_

#include "base/macros.h"
#include "ios/web/public/web_state_observer.h"
#import "ios/web/public/web_state_user_data.h"

class PrefService;

namespace user_prefs {
class PrefRegistrySyncable;
}

namespace web {
class WebState;
}  // namespace web

enum Zoom {
  ZOOM_OUT = -1,
  ZOOM_RESET = 0,
  ZOOM_IN = 1,
};

// Adjusts font size of web page by mapping
// |UIApplication.sharedApplication.preferredContentSizeCategory| to a scaling
// percentage and setting it to "-webkit-font-size-adjust" style on <body> when
// the page is successfully loaded or system font size changes.
class FontSizeTabHelper : public web::WebStateObserver,
                          public web::WebStateUserData<FontSizeTabHelper> {
 public:
  ~FontSizeTabHelper() override;

  // Performs a zoom in the given direction on the WebState this is attached to.
  void UserZoom(Zoom zoom);

  // Returns whether the user can still zoom in. (I.e., They have not reached
  // the max zoom level.).
  bool CanUserZoomIn() const;
  // Returns whether the user can still zoom out. (I.e., They have not reached
  // the min zoom level.).
  bool CanUserZoomOut() const;

  static void RegisterBrowserStatePrefs(
      user_prefs::PrefRegistrySyncable* registry);

 private:
  friend class web::WebStateUserData<FontSizeTabHelper>;

  explicit FontSizeTabHelper(web::WebState* web_state);

  // Sets font size in web page by scaling percentage.
  void SetPageFontSize(int size);

  // Returns the true font size in scaling percentage (e.g. 150 for
  // 150%) taking all sources into account (system level and user zoom).
  int GetFontSize() const;

  PrefService* GetPrefService() const;

  base::Optional<double> NewMultiplierAfterZoom(Zoom zoom) const;
  // Returns the current user zoom multiplier (i.e. not counting any additional
  // zoom due to the system accessibility settings).
  double GetCurrentUserZoomMultiplier() const;
  void StoreCurrentUserZoomMultiplier(double multiplier);
  std::string GetCurrentUserZoomMultiplierKey() const;

  bool tab_helper_has_zoomed_ = false;

  // web::WebStateObserver overrides:
  void PageLoaded(
      web::WebState* web_state,
      web::PageLoadCompletionStatus load_completion_status) override;
  void WebStateDestroyed(web::WebState* web_state) override;

  // Observer id returned by registering at NSNotificationCenter.
  id content_size_did_change_observer_ = nil;

  // WebState this tab helper is attached to.
  web::WebState* web_state_ = nullptr;

  WEB_STATE_USER_DATA_KEY_DECL();

  DISALLOW_COPY_AND_ASSIGN(FontSizeTabHelper);
};

#endif  // IOS_CHROME_BROWSER_WEB_FONT_SIZE_TAB_HELPER_H_
