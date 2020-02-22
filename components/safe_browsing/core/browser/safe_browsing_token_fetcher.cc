// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/safe_browsing/core/browser/safe_browsing_token_fetcher.h"

#include "base/bind.h"
#include "base/memory/weak_ptr.h"
#include "base/optional.h"
#include "base/task/post_task.h"
#include "base/time/time.h"
#include "components/safe_browsing/core/common/thread_utils.h"
#include "components/signin/public/identity_manager/access_token_fetcher.h"
#include "components/signin/public/identity_manager/access_token_info.h"
#include "components/signin/public/identity_manager/identity_manager.h"
#include "google_apis/gaia/core_account_id.h"
#include "google_apis/gaia/google_service_auth_error.h"

namespace safe_browsing {

namespace {

// TODO(crbug.com/1041912): Finalize the API scope.
const char kAPIScope[] = "";

const int kTimeoutDelaySeconds = 5 * 60;

}  // namespace

SafeBrowsingTokenFetcher::SafeBrowsingTokenFetcher(
    signin::IdentityManager* identity_manager,
    signin::ConsentLevel consent,
    Callback callback)
    : callback_(std::move(callback)), weak_ptr_factory_(this) {
  DCHECK(CurrentlyOnThread(ThreadID::UI));
  CoreAccountId account_id = identity_manager->GetPrimaryAccountId(consent);
  token_fetcher_ = identity_manager->CreateAccessTokenFetcherForAccount(
      account_id, "safe_browsing_service", {kAPIScope},
      base::BindOnce(&SafeBrowsingTokenFetcher::OnTokenFetched,
                     weak_ptr_factory_.GetWeakPtr()),
      signin::AccessTokenFetcher::Mode::kImmediate);
  base::PostDelayedTask(
      FROM_HERE, CreateTaskTraits(ThreadID::UI),
      base::BindOnce(&SafeBrowsingTokenFetcher::OnTokenTimeout,
                     weak_ptr_factory_.GetWeakPtr()),
      base::TimeDelta::FromSeconds(kTimeoutDelaySeconds));
}

SafeBrowsingTokenFetcher::~SafeBrowsingTokenFetcher() = default;

void SafeBrowsingTokenFetcher::OnTokenFetched(
    GoogleServiceAuthError error,
    signin::AccessTokenInfo access_token_info) {
  if (error.state() == GoogleServiceAuthError::NONE)
    Finish(access_token_info);
  else
    Finish(base::nullopt);
}

void SafeBrowsingTokenFetcher::OnTokenTimeout() {
  Finish(base::nullopt);
}

void SafeBrowsingTokenFetcher::Finish(
    base::Optional<signin::AccessTokenInfo> token_info) {
  std::move(callback_).Run(token_info);

  // Cancel any pending callbacks
  weak_ptr_factory_.InvalidateWeakPtrs();
}

}  // namespace safe_browsing
