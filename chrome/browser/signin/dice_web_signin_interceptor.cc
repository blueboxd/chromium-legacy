// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/signin/dice_web_signin_interceptor.h"

#include "base/check.h"
#include "chrome/browser/signin/identity_manager_factory.h"
#include "chrome/browser/signin/signin_features.h"
#include "components/signin/public/identity_manager/identity_manager.h"

DiceWebSigninInterceptor::DiceWebSigninInterceptor(Profile* profile)
    : profile_(profile),
      identity_manager_(IdentityManagerFactory::GetForProfile(profile)) {
  DCHECK(profile_);
  DCHECK(identity_manager_);
}

DiceWebSigninInterceptor::~DiceWebSigninInterceptor() = default;

void DiceWebSigninInterceptor::MaybeInterceptWebSignin(
    content::WebContents* web_contents,
    CoreAccountId account_id,
    bool is_new_account) {
  if (!base::FeatureList::IsEnabled(kDiceWebSigninInterceptionFeature))
    return;
  if (is_interception_in_progress_)
    return;  // Multiple concurrent interceptions are not supported.
  if (!is_new_account)
    return;  // Do not intercept reauth.
  if (identity_manager_->GetAccountsWithRefreshTokens().size() <= 1u)
    return;  // Do not intercept the first account.

  // TODO(https://crbug.com/1076880): implement interception
  // - fetch user info
  // - show interception UI
  // - profile creation
  // - move the account
  // - move the tab
}
