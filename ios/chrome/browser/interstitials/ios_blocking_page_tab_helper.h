// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_INTERSTITIALS_IOS_BLOCKING_PAGE_TAB_HELPER_H_
#define IOS_CHROME_BROWSER_INTERSTITIALS_IOS_BLOCKING_PAGE_TAB_HELPER_H_

#include "ios/chrome/browser/interstitials/ios_security_interstitial_page.h"
#include "ios/web/public/web_state_observer.h"
#import "ios/web/public/web_state_user_data.h"

namespace web {
class WebState;
}  // namespace web

// Helps manage IOSSecurityInterstitialPage lifetime independent from
// interstitial code. Stores an IOSSecurityInterstitialPage while an SSL error
// is currently being shown, then cleans it up when the user navigates away
// from the SSL error.
class IOSBlockingPageTabHelper
    : public web::WebStateObserver,
      public web::WebStateUserData<IOSBlockingPageTabHelper> {
 public:
  ~IOSBlockingPageTabHelper() override;

  // Associates |blocking_page| with an IOSBlockingPageTabHelper to manage the
  // |blocking_page|'s lifetime.
  static void AssociateBlockingPage(
      web::WebState* web_state,
      int64_t navigation_id,
      std::unique_ptr<IOSSecurityInterstitialPage> blocking_page);

  // Returns the blocking page showing on the current tab.
  IOSSecurityInterstitialPage* GetCurrentBlockingPage(
      web::WebState* web_state) const;

  // web::WebStateObserver implementation.
  void DidFinishNavigation(web::WebState* web_state,
                           web::NavigationContext* navigation_context) override;
  void WebStateDestroyed(web::WebState* web_state) override;

 private:
  WEB_STATE_USER_DATA_KEY_DECL();

  explicit IOSBlockingPageTabHelper(web::WebState* web_state);
  DISALLOW_COPY_AND_ASSIGN(IOSBlockingPageTabHelper);

  friend class web::WebStateUserData<IOSBlockingPageTabHelper>;

  void SetBlockingPage(
      int64_t navigation_id,
      std::unique_ptr<IOSSecurityInterstitialPage> blocking_page);

  // Handler for "blockingPage.*" JavaScript command. Dispatch to more specific
  // handler.
  void OnBlockingPageCommand(const base::DictionaryValue& message,
                             const GURL& url,
                             bool user_is_interacting,
                             web::WebFrame* sender_frame);

  // Keeps track of blocking pages for navigations that have encountered
  // certificate errors in this WebState. When a navigation commits, the
  // corresponding blocking page is moved out and stored in
  // |blocking_page_for_currently_committed_navigation_|.
  std::map<int64_t, std::unique_ptr<IOSSecurityInterstitialPage>>
      blocking_pages_for_navigations_;
  // Keeps track of the blocking page for the currently committed navigation, if
  // there is one. The value is replaced (if the new committed navigation has a
  // blocking page) or reset on every committed navigation.
  std::unique_ptr<IOSSecurityInterstitialPage>
      blocking_page_for_currently_committed_navigation_;

  // The WebState this instance is observing. Will be null after
  // WebStateDestroyed has been called.
  web::WebState* web_state_ = nullptr;

  // Subscription for JS messages.
  std::unique_ptr<web::WebState::ScriptCommandSubscription> subscription_;
  base::WeakPtrFactory<IOSBlockingPageTabHelper> weak_factory_{this};
};

#endif  // IOS_CHROME_BROWSER_INTERSTITIALS_IOS_BLOCKING_PAGE_TAB_HELPER_H_
