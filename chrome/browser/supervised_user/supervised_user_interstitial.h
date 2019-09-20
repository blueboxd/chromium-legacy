// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SUPERVISED_USER_SUPERVISED_USER_INTERSTITIAL_H_
#define CHROME_BROWSER_SUPERVISED_USER_SUPERVISED_USER_INTERSTITIAL_H_

#include <string>

#include "base/callback_forward.h"
#include "base/macros.h"
#include "chrome/browser/supervised_user/supervised_user_error_page/supervised_user_error_page.h"
#include "url/gurl.h"

namespace content {
class WebContents;
}

class Profile;

// This class is used by SupervisedUserNavigationObserver to handle requests
// from supervised user error page. The error page is shown when a page is
// blocked because it is on a blacklist (in "allow everything" mode), not on any
// whitelist (in "allow only specified sites" mode), or doesn't pass safe
// search.
class SupervisedUserInterstitial {
 public:
  ~SupervisedUserInterstitial();

  static std::unique_ptr<SupervisedUserInterstitial> Create(
      content::WebContents* web_contents,
      const GURL& url,
      supervised_user_error_page::FilteringBehaviorReason reason);

  static std::string GetHTMLContents(
      Profile* profile,
      supervised_user_error_page::FilteringBehaviorReason reason);

  void GoBack();
  void RequestPermission(base::OnceCallback<void(bool)> callback);
  void ShowFeedback();

 private:
  SupervisedUserInterstitial(
      content::WebContents* web_contents,
      const GURL& url,
      supervised_user_error_page::FilteringBehaviorReason reason);

  // Moves away from the page behind the interstitial when not proceeding with
  // the request.
  void MoveAwayFromCurrentPage();

  void OnInterstitialDone();

  // Owns the interstitial, which owns us.
  content::WebContents* web_contents_;

  Profile* profile_;

  GURL url_;
  supervised_user_error_page::FilteringBehaviorReason reason_;

  DISALLOW_COPY_AND_ASSIGN(SupervisedUserInterstitial);
};

#endif  // CHROME_BROWSER_SUPERVISED_USER_SUPERVISED_USER_INTERSTITIAL_H_
