// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_CHANGE_PASSWORD_URL_SERVICE_H_
#define COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_CHANGE_PASSWORD_URL_SERVICE_H_

#include "base/callback_forward.h"
#include "components/keyed_service/core/keyed_service.h"

class GURL;

namespace url {
class Origin;
}

namespace password_manager {

class ChangePasswordUrlService : public KeyedService {
 public:
  using UrlCallback = base::OnceCallback<void(GURL)>;
  // Initializes the service.
  virtual void Initialize() = 0;
  // Returns the change password URL for `origin` via `callback`.
  virtual void GetChangePasswordUrl(const url::Origin& origin,
                                    UrlCallback callback) = 0;
};

}  // namespace password_manager

#endif  // COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_CHANGE_PASSWORD_URL_SERVICE_H_
