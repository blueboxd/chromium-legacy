// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_PASSWORDS_WELL_KNOWN_CHANGE_PASSWORD_NAVIGATION_THROTTLE_H_
#define CHROME_BROWSER_UI_PASSWORDS_WELL_KNOWN_CHANGE_PASSWORD_NAVIGATION_THROTTLE_H_

#include <memory>

#include "content/public/browser/navigation_throttle.h"

namespace content {
class NavigationHandle;
}  // namespace content

// This NavigationThrottle checks whether a site supports the
// .well-known/change-password url. To check whether a site supports the
// change-password url, we also request a .well-known path that is defined to
// return a 404. When that one returns a 404 and the change password path a 200
// we assume the site supports the change-password url. If the site does not
// support the change password url, the user gets redirected to the base path
// '/'.
class WellKnownChangePasswordNavigationThrottle
    : public content::NavigationThrottle {
 public:
  ~WellKnownChangePasswordNavigationThrottle() override;

  static std::unique_ptr<WellKnownChangePasswordNavigationThrottle>
  MaybeCreateThrottleFor(content::NavigationHandle* handle);

  // We don't need to override WillRedirectRequest since a redirect is the
  // expected behaviour and does not need manual intervention.
  // content::NavigationThrottle:
  ThrottleCheckResult WillStartRequest() override;
  ThrottleCheckResult WillFailRequest() override;
  ThrottleCheckResult WillProcessResponse() override;
  const char* GetNameForLogging() override;

 private:
  explicit WellKnownChangePasswordNavigationThrottle(
      content::NavigationHandle* handle);
};

#endif  // CHROME_BROWSER_UI_PASSWORDS_WELL_KNOWN_CHANGE_PASSWORD_NAVIGATION_THROTTLE_H_
