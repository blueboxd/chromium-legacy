// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/ambient/ambient_access_token_controller.h"

#include <algorithm>
#include <utility>

#include "ash/public/cpp/ambient/ambient_client.h"
#include "base/rand_util.h"
#include "base/time/time.h"

namespace ash {

namespace {

constexpr base::TimeDelta kMinTokenRefreshDelay =
    base::TimeDelta::FromMilliseconds(1000);
constexpr base::TimeDelta kMaxTokenRefreshDelay =
    base::TimeDelta::FromMilliseconds(60 * 1000);

// The buffer time to use the access token.
constexpr base::TimeDelta kTokenExpirationTimeBuffer =
    base::TimeDelta::FromMinutes(10);

}  // namespace

AmbientAccessTokenController::AmbientAccessTokenController() = default;

AmbientAccessTokenController::~AmbientAccessTokenController() = default;

void AmbientAccessTokenController::RequestAccessToken(
    AccessTokenCallback callback) {
  // |token_refresh_timer_| may become stale during sleeping.
  if (token_refresh_timer_.IsRunning())
    token_refresh_timer_.AbandonAndStop();

  if (!access_token_.empty()) {
    DCHECK(!has_pending_request_);

    // Return the token if there is enough time to use the access token when
    // requested.
    if (expiration_time_ - base::Time::Now() > kTokenExpirationTimeBuffer) {
      RunCallback(std::move(callback));
      return;
    }

    access_token_ = std::string();
    expiration_time_ = base::Time::Now();
  }

  callbacks_.emplace_back(std::move(callback));

  if (has_pending_request_)
    return;

  RefreshAccessToken();
}

void AmbientAccessTokenController::RefreshAccessToken() {
  DCHECK(!token_refresh_timer_.IsRunning());

  has_pending_request_ = true;
  AmbientClient::Get()->RequestAccessToken(
      base::BindOnce(&AmbientAccessTokenController::AccessTokenRefreshed,
                     weak_factory_.GetWeakPtr()));
}

void AmbientAccessTokenController::AccessTokenRefreshed(
    const std::string& gaia_id,
    const std::string& access_token,
    const base::Time& expiration_time) {
  has_pending_request_ = false;

  if (gaia_id.empty() || access_token.empty()) {
    RetryRefreshAccessToken();
    return;
  }

  VLOG(1) << "Access token fetched.";
  gaia_id_ = gaia_id;
  access_token_ = access_token;
  expiration_time_ = expiration_time;
  NotifyAccessTokenRefreshed();
}

void AmbientAccessTokenController::RetryRefreshAccessToken() {
  base::TimeDelta backoff_delay =
      std::min(kMinTokenRefreshDelay *
                   (1 << (token_refresh_error_backoff_factor - 1)),
               kMaxTokenRefreshDelay) +
      base::RandDouble() * kMinTokenRefreshDelay;

  if (backoff_delay < kMaxTokenRefreshDelay)
    ++token_refresh_error_backoff_factor;

  token_refresh_timer_.Start(
      FROM_HERE, backoff_delay,
      base::BindOnce(&AmbientAccessTokenController::RefreshAccessToken,
                     base::Unretained(this)));
}

void AmbientAccessTokenController::NotifyAccessTokenRefreshed() {
  for (auto& callback : callbacks_)
    RunCallback(std::move(callback));

  callbacks_.clear();
}

void AmbientAccessTokenController::RunCallback(AccessTokenCallback callback) {
  DCHECK(!gaia_id_.empty());
  DCHECK(!access_token_.empty());
  std::move(callback).Run(gaia_id_, access_token_);
}

}  // namespace ash
