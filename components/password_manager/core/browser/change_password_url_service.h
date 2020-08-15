// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_CHANGE_PASSWORD_URL_SERVICE_H_
#define COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_CHANGE_PASSWORD_URL_SERVICE_H_

#include "components/keyed_service/core/keyed_service.h"

class GURL;

namespace password_manager {

class ChangePasswordUrlService : public KeyedService {
 public:
  // Prefetch the change password URLs that point to the password change form
  // from gstatic.
  virtual void PrefetchURLs() = 0;
  // Returns a change password url for a given |origin| using eTLD+1. If no
  // override is available or the fetch is not completed yet an empty GURL is
  // returned.
  virtual GURL GetChangePasswordUrl(const GURL& url) = 0;
};

}  // namespace password_manager

#endif  // COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_CHANGE_PASSWORD_URL_SERVICE_H_
