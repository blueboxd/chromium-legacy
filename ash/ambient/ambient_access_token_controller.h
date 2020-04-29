// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_AMBIENT_AMBIENT_ACCESS_TOKEN_CONTROLLER_H_
#define ASH_AMBIENT_AMBIENT_ACCESS_TOKEN_CONTROLLER_H_

#include <string>
#include <vector>

#include "ash/ash_export.h"
#include "base/memory/weak_ptr.h"
#include "base/time/time.h"
#include "base/timer/timer.h"

namespace ash {

// A class to manage the access token for ambient mode. Request will be async
// and will be returned as soon as the token is refreshed. If the token has
// already been refreshed, request call will be returned immediately.
class ASH_EXPORT AmbientAccessTokenController {
 public:
  using AccessTokenCallback =
      base::OnceCallback<void(const std::string& gaia_id,
                              const std::string& access_token)>;

  AmbientAccessTokenController();
  AmbientAccessTokenController(const AmbientAccessTokenController&) = delete;
  AmbientAccessTokenController& operator=(const AmbientAccessTokenController&) =
      delete;
  ~AmbientAccessTokenController();

  void RequestAccessToken(AccessTokenCallback callback);

 private:
  friend class AmbientAshTestBase;

  void RefreshAccessToken();
  void AccessTokenRefreshed(const std::string& gaia_id,
                            const std::string& access_token,
                            const base::Time& expiration_time);
  void RetryRefreshAccessToken();
  void NotifyAccessTokenRefreshed();
  void RunCallback(AccessTokenCallback callback);

  std::string gaia_id_;
  std::string access_token_;

  // The expiration time of the |access_token_|.
  base::Time expiration_time_;

  // True if has already sent access token request and waiting for result.
  bool has_pending_request_ = false;

  base::OneShotTimer token_refresh_timer_;
  int token_refresh_error_backoff_factor = 1;

  std::vector<AccessTokenCallback> callbacks_;

  base::WeakPtrFactory<AmbientAccessTokenController> weak_factory_{this};
};

}  // namespace ash

#endif  // ASH_AMBIENT_AMBIENT_ACCESS_TOKEN_CONTROLLER_H_
