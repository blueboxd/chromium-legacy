// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SIGNIN_DICE_WEB_SIGNIN_INTERCEPTOR_H_
#define CHROME_BROWSER_SIGNIN_DICE_WEB_SIGNIN_INTERCEPTOR_H_

#include "base/feature_list.h"
#include "components/keyed_service/core/keyed_service.h"
#include "content/public/browser/web_contents_observer.h"
#include "google_apis/gaia/core_account_id.h"

namespace content {
class WebContents;
}

namespace signin {
class IdentityManager;
}

class Profile;

// Called after web signed in, after a successful token exchange through Dice.
// The DiceWebSigninInterceptor may offer the user to create a new profile or
// switch to another existing profile.
class DiceWebSigninInterceptor : public KeyedService,
                                 public content::WebContentsObserver {
 public:
  explicit DiceWebSigninInterceptor(Profile* profile);
  ~DiceWebSigninInterceptor() override;

  DiceWebSigninInterceptor(const DiceWebSigninInterceptor&) = delete;
  DiceWebSigninInterceptor& operator=(const DiceWebSigninInterceptor&) = delete;

  // |web_contents| is the tab where the signin event happened. It must belong
  // to the profile associated with this service. It may be nullptr if the tab
  // was closed.
  void MaybeInterceptWebSignin(content::WebContents* web_contents,
                               CoreAccountId account_id,
                               bool is_new_account);

 private:
  Profile* const profile_;
  signin::IdentityManager* const identity_manager_;

  CoreAccountId account_id_;
  bool is_interception_in_progress_ = false;
};

#endif  // CHROME_BROWSER_SIGNIN_DICE_WEB_SIGNIN_INTERCEPTOR_H_
