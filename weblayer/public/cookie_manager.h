// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WEBLAYER_PUBLIC_COOKIE_MANAGER_H_
#define WEBLAYER_PUBLIC_COOKIE_MANAGER_H_

#include <string>

#include "base/callback_forward.h"

class GURL;

namespace weblayer {

class CookieManager {
 public:
  virtual ~CookieManager() = default;

  // Sets a cookie for the given URL.
  using SetCookieCallback = base::OnceCallback<void(bool)>;
  virtual void SetCookie(const GURL& url,
                         const std::string& value,
                         SetCookieCallback callback) = 0;

  // Gets the cookies for the given URL.
  using GetCookieCallback = base::OnceCallback<void(const std::string&)>;
  virtual void GetCookie(const GURL& url, GetCookieCallback callback) = 0;
};

}  // namespace weblayer

#endif  // WEBLAYER_PUBLIC_COOKIE_MANAGER_H_
