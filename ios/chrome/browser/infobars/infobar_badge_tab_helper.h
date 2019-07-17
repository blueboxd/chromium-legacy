// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_INFOBARS_INFOBAR_BADGE_TAB_HELPER_H_
#define IOS_CHROME_BROWSER_INFOBARS_INFOBAR_BADGE_TAB_HELPER_H_

#include <unordered_map>

#include "base/scoped_observer.h"
#import "ios/web/public/web_state/web_state_user_data.h"

#include "components/infobars/core/infobar_manager.h"
#import "ios/chrome/browser/infobars/infobar_type.h"
#import "ios/chrome/browser/ui/badges/badge_item.h"

namespace web {
class WebState;
}

@protocol InfobarBadgeTabHelperDelegate;
@protocol LegacyInfobarBadgeTabHelperDelegate;
@class InfobarBadgeModel;

// TabHelper that observes InfoBarManager. It updates an InfobarBadge delegate
// for relevant Infobar changes.
class InfobarBadgeTabHelper
    : public infobars::InfoBarManager::Observer,
      public web::WebStateUserData<InfobarBadgeTabHelper> {
 public:
  // Creates the tab helper for |web_state| if it does not exist.
  static void CreateForWebState(web::WebState* web_state);
  // Sets the InfobarBadgeTabHelperDelegate to |delegate|.
  void SetDelegate(id<InfobarBadgeTabHelperDelegate> delegate);
  // Updates Infobar badge for the case where Infobar of |infobar_type| was
  // accepted.
  void UpdateBadgeForInfobarAccepted(InfobarType infobar_type);

  // Returns all BadgeItems for the TabHelper Webstate.
  std::vector<id<BadgeItem>> GetInfobarBadgeItems();

  ~InfobarBadgeTabHelper() override;

 private:
  friend class web::WebStateUserData<InfobarBadgeTabHelper>;
  // Constructor.
  explicit InfobarBadgeTabHelper(web::WebState* web_state);
  // InfoBarManagerObserver implementation.
  void OnInfoBarAdded(infobars::InfoBar* infobar) override;
  void OnInfoBarRemoved(infobars::InfoBar* infobar, bool animate) override;
  void OnManagerShuttingDown(infobars::InfoBarManager* manager) override;
  // Updates the badge delegate for |infobar|.
  void UpdateBadgeForInfobar(infobars::InfoBar* infobar, bool display);

  // Manages this object as an observer of infobars.
  ScopedObserver<infobars::InfoBarManager, infobars::InfoBarManager::Observer>
      infobar_observer_;

  // Delegate which displays the Infobar badge.
  __weak id<InfobarBadgeTabHelperDelegate> delegate_ = nil;
  // Holds the state of each displaying badge keyed by its InfobarType.
  std::unordered_map<InfobarType, InfobarBadgeModel*> infobar_badge_models_;

  WEB_STATE_USER_DATA_KEY_DECL();
  DISALLOW_COPY_AND_ASSIGN(InfobarBadgeTabHelper);
};

#endif  // IOS_CHROME_BROWSER_INFOBARS_INFOBAR_BADGE_TAB_HELPER_H_
