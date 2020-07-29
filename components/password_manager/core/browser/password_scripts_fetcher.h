// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_PASSWORD_SCRIPTS_FETCHER_H_
#define COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_PASSWORD_SCRIPTS_FETCHER_H_

#include "base/callback_forward.h"
#include "components/keyed_service/core/keyed_service.h"

namespace url {
class Origin;
}

namespace password_manager {

// Abstract interface to fetch the list of password scripts.
class PasswordScriptsFetcher : public KeyedService {
 public:
  using ResponseCallback = base::OnceCallback<void(bool)>;
  // Triggers pre-fetching the list of scripts. Should be called from UI
  // preceding Bulk Check.
  virtual void PrewarmCache() = 0;
  // Reports metrics about the cache readiness. Should be called right before
  // the first call of |GetPasswordScriptAvailability| within a given bulk
  // check.
  virtual void ReportCacheReadinessMetric() const = 0;
  // Returns whether there is a password change script for |origin| via
  // |callback|. If the cache was never set or is stale, it triggers a new
  // network request (but doesn't trigger a duplicate request if another request
  // is in-flight) and enqueues |callback|. Otherwise, it runs the callback
  // immediately. In case of an network error, the verdict will default to no
  // script being available.
  virtual void GetPasswordScriptAvailability(const url::Origin& origin,
                                             ResponseCallback callback) = 0;
};

}  // namespace password_manager

#endif  // COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_PASSWORD_SCRIPTS_FETCHER_H_
