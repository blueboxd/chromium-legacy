// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_INFOBARS_INFOBAR_OVERLAY_TAB_HELPER_H_
#define IOS_CHROME_BROWSER_INFOBARS_INFOBAR_OVERLAY_TAB_HELPER_H_

#include <memory>

#include "base/scoped_observer.h"
#include "components/infobars/core/infobar_manager.h"
#import "ios/web/public/web_state_user_data.h"

class InfobarBannerOverlayRequestFactory;
class OverlayRequestQueue;

// Helper class that creates OverlayRequests for the banner UI for InfoBars
// added to an InfoBarManager.
class InfobarOverlayTabHelper
    : public web::WebStateUserData<InfobarOverlayTabHelper> {
 public:
  ~InfobarOverlayTabHelper() override;

  // Creates an InfobarOverlayTabHelper scoped to |web_state| that creates
  // OverlayRequests for InfoBars added to |web_state|'s InfoBarManagerImpl
  // using |request_factory|.
  static void CreateForWebState(
      web::WebState* web_state,
      std::unique_ptr<InfobarBannerOverlayRequestFactory> request_factory);

 private:
  InfobarOverlayTabHelper(
      web::WebState* web_state,
      std::unique_ptr<InfobarBannerOverlayRequestFactory> request_factory);
  friend class web::WebStateUserData<InfobarOverlayTabHelper>;
  WEB_STATE_USER_DATA_KEY_DECL();

  // Helper object that schedules OverlayRequests for the banner UI for InfoBars
  // added to a WebState's InfoBarManager.
  class OverlayRequestScheduler : public infobars::InfoBarManager::Observer {
   public:
    OverlayRequestScheduler(
        web::WebState* web_state,
        std::unique_ptr<InfobarBannerOverlayRequestFactory> request_factory);
    ~OverlayRequestScheduler() override;

   private:
    // infobars::InfoBarManager::Observer:
    void OnInfoBarAdded(infobars::InfoBar* infobar) override;
    void OnManagerShuttingDown(infobars::InfoBarManager* manager) override;

   private:
    // The request queue for the owning WebState.
    OverlayRequestQueue* queue_ = nullptr;
    // The request factory passed on initialization.  Used to create
    // OverlayRequests for InfoBars added to the InfoBarManager.
    std::unique_ptr<InfobarBannerOverlayRequestFactory> request_factory_;

    ScopedObserver<infobars::InfoBarManager, infobars::InfoBarManager::Observer>
        scoped_observer_;
  };

  // The scheduler used to create OverlayRequests for InfoBars added to the
  // corresponding WebState's InfoBarManagerImpl.
  OverlayRequestScheduler overlay_request_scheduler_;
};
#endif  // IOS_CHROME_BROWSER_INFOBARS_INFOBAR_OVERLAY_TAB_HELPER_H_
