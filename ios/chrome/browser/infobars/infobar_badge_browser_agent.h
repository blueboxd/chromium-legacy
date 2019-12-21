// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_INFOBARS_INFOBAR_BADGE_BROWSER_AGENT_H_
#define IOS_CHROME_BROWSER_INFOBARS_INFOBAR_BADGE_BROWSER_AGENT_H_

#include "base/scoped_observer.h"
#import "ios/chrome/browser/infobars/infobar_type.h"
#include "ios/chrome/browser/main/browser_user_data.h"
#include "ios/chrome/browser/overlays/public/overlay_presenter.h"
#include "ios/chrome/browser/overlays/public/overlay_presenter_observer.h"

class InfobarBadgeTabHelper;
namespace web {
class WebState;
}

// Browser agent that updates InfobarTabHelpers for observed infobar overlay
// events.
class InfobarBadgeBrowserAgent
    : public BrowserUserData<InfobarBadgeBrowserAgent> {
 public:
  ~InfobarBadgeBrowserAgent() override;

 private:
  BROWSER_USER_DATA_KEY_DECL();
  friend class BrowserUserData<InfobarBadgeBrowserAgent>;
  explicit InfobarBadgeBrowserAgent(Browser* browser);

  // Returns the badge tab helper for the active WebState.
  InfobarBadgeTabHelper* GetBadgeCurrentTabHelper() const;

  // Called when the banner UI for |request| was presented or dismissed.
  void OnInfobarBannerPresented(OverlayRequest* request);
  void OnInfobarBannerDismissed(OverlayRequest* request);

  // Helper object that observes the presentation of infobar overlays.
  class InfobarOverlayObserver : public OverlayPresenterObserver {
   public:
    InfobarOverlayObserver(InfobarBadgeBrowserAgent* browser_agent,
                           Browser* browser);
    ~InfobarOverlayObserver() override;

   private:
    // OverlayPresenterObserver:
    void DidShowOverlay(OverlayPresenter* presenter,
                        OverlayRequest* request) override;
    void DidHideOverlay(OverlayPresenter* presenter,
                        OverlayRequest* request) override;
    void OverlayPresenterDestroyed(OverlayPresenter* presenter) override;

   private:
    ScopedObserver<OverlayPresenter, OverlayPresenterObserver> scoped_observer_;
    InfobarBadgeBrowserAgent* browser_agent_;
  };

  InfobarOverlayObserver infobar_overlay_observer_;
  WebStateList* web_state_list_ = nullptr;
};

#endif  // IOS_CHROME_BROWSER_INFOBARS_INFOBAR_BADGE_BROWSER_AGENT_H_
