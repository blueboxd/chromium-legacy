// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_PUBLIC_CPP_AMBIENT_AMBIENT_CLIENT_H_
#define ASH_PUBLIC_CPP_AMBIENT_AMBIENT_CLIENT_H_

#include <memory>
#include <string>

#include "ash/public/cpp/ash_public_export.h"
#include "base/callback_forward.h"
#include "base/memory/scoped_refptr.h"

namespace base {
class Time;
}  // namespace base

namespace network {
class SharedURLLoaderFactory;
}  // namespace network

namespace ash {

// Interface for a class which provides profile related info in the ambient mode
// in ash.
class ASH_PUBLIC_EXPORT AmbientClient {
 public:
  using GetAccessTokenCallback =
      base::OnceCallback<void(const std::string& gaia_id,
                              const std::string& access_token,
                              const base::Time& expiration_time)>;

  static AmbientClient* Get();

  // Return whether the ambient mode is allowed for the active user.
  virtual bool IsAmbientModeAllowedForActiveUser() = 0;

  // Return the gaia and access token associated with the active user's profile.
  virtual void RequestAccessToken(GetAccessTokenCallback callback) = 0;

  // Return the URL loader factory associated with the active user's profile.
  virtual scoped_refptr<network::SharedURLLoaderFactory>
  GetURLLoaderFactory() = 0;

 protected:
  AmbientClient();
  AmbientClient(const AmbientClient&) = delete;
  AmbientClient& operator=(const AmbientClient&) = delete;
  virtual ~AmbientClient();
};

}  // namespace ash

#endif  // ASH_PUBLIC_CPP_AMBIENT_AMBIENT_CLIENT_H_
