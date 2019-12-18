// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_INFOBARS_OVERLAYS_INFOBAR_OVERLAY_TAB_HELPER_H_
#define IOS_CHROME_BROWSER_INFOBARS_OVERLAYS_INFOBAR_OVERLAY_TAB_HELPER_H_

#include <memory>

#include "base/scoped_observer.h"
#include "components/infobars/core/infobar_manager.h"
#import "ios/chrome/browser/infobars/overlays/infobar_overlay_request_inserter.h"
#import "ios/web/public/web_state_user_data.h"

class InfobarOverlayRequestFactory;
class InfobarOverlayRequestInserter;

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
      std::unique_ptr<InfobarOverlayRequestFactory> request_factory);

 private:
  InfobarOverlayTabHelper(
      web::WebState* web_state,
      std::unique_ptr<InfobarOverlayRequestFactory> request_factory);
  friend class web::WebStateUserData<InfobarOverlayTabHelper>;
  WEB_STATE_USER_DATA_KEY_DECL();

  // Getter for the request inserter.
  const InfobarOverlayRequestInserter* request_inserter() const {
    return &request_inserter_;
  }

  // Helper object that schedules OverlayRequests for the banner UI for InfoBars
  // added to a WebState's InfoBarManager.
  class OverlayRequestScheduler : public infobars::InfoBarManager::Observer {
   public:
    OverlayRequestScheduler(web::WebState* web_state,
                            InfobarOverlayTabHelper* tab_helper);
    ~OverlayRequestScheduler() override;

   private:
    // infobars::InfoBarManager::Observer:
    void OnInfoBarAdded(infobars::InfoBar* infobar) override;
    void OnManagerShuttingDown(infobars::InfoBarManager* manager) override;

   private:
    // The owning tab helper.
    InfobarOverlayTabHelper* tab_helper_ = nullptr;
    ScopedObserver<infobars::InfoBarManager, infobars::InfoBarManager::Observer>
        scoped_observer_;
  };

  // The inserter used to add infobar OverlayRequests to the WebState's queue.
  InfobarOverlayRequestInserter request_inserter_;
  // The scheduler used to create OverlayRequests for InfoBars added to the
  // corresponding WebState's InfoBarManagerImpl.
  OverlayRequestScheduler request_scheduler_;
};
#endif  // IOS_CHROME_BROWSER_INFOBARS_OVERLAYS_INFOBAR_OVERLAY_TAB_HELPER_H_
