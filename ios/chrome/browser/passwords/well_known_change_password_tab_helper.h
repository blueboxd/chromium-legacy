// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#ifndef IOS_CHROME_BROWSER_PASSWORDS_WELL_KNOWN_CHANGE_PASSWORD_TAB_HELPER_H_
#define IOS_CHROME_BROWSER_PASSWORDS_WELL_KNOWN_CHANGE_PASSWORD_TAB_HELPER_H_

#include "ios/web/public/navigation/web_state_policy_decider.h"
#include "ios/web/public/web_state_observer.h"
#import "ios/web/public/web_state_user_data.h"

namespace password_manager {

// This TabHelper checks whether a site supports the .well-known/change-password
// url. To check whether a site supports the change-password url the TabHelper
// also request a .well-known path that is defined to return a 404. When that
// one returns 404 and the change password path 2XX we assume the site supports
// the change-password url. If the site does not support the change password
// url, the user gets redirected to the base path '/'. If the sites supports the
// standard, the request is allowed and the navigation is not changed.
class WellKnownChangePasswordTabHelper
    : public web::WebStatePolicyDecider,
      public web::WebStateObserver,
      public web::WebStateUserData<WellKnownChangePasswordTabHelper> {
 public:
  ~WellKnownChangePasswordTabHelper() override;
  PolicyDecision ShouldAllowRequest(NSURLRequest* request,
                                    const RequestInfo& request_info) override;
  void ShouldAllowResponse(
      NSURLResponse* response,
      bool for_main_frame,
      web::WebStatePolicyDecider::PolicyDecisionCallback callback) override;
  void DidRedirectNavigation(
      web::WebState* web_state,
      web::NavigationContext* navigation_context) override;
  void WebStateDestroyed() override;
  void WebStateDestroyed(web::WebState* web_state) override;

 private:
  explicit WellKnownChangePasswordTabHelper(web::WebState* web_state);

  friend class web::WebStateUserData<WellKnownChangePasswordTabHelper>;
  web::WebState* web_state_ = nullptr;

  WEB_STATE_USER_DATA_KEY_DECL();
};

}  // namespace password_manager

#endif  // IOS_CHROME_BROWSER_PASSWORDS_WELL_KNOWN_CHANGE_PASSWORD_TAB_HELPER_H_
