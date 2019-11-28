// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_CHILD_ACCOUNTS_CHILD_USER_SERVICE_H_
#define CHROME_BROWSER_CHROMEOS_CHILD_ACCOUNTS_CHILD_USER_SERVICE_H_

#include <memory>
#include "components/keyed_service/core/keyed_service.h"

namespace base {
class TimeDelta;
}  // namespace base

namespace content {
class BrowserContext;
}  // namespace content

class GURL;

namespace chromeos {
class AppTimeController;

// Facade that exposes child user related functionality on Chrome OS.
// TODO(crbug.com/1022231): Migrate ConsumerStatusReportingService,
// EventBasedStatusReporting and ScreenTimeController to ChildUserService.
class ChildUserService : public KeyedService {
 public:
  explicit ChildUserService(content::BrowserContext* context);
  ChildUserService(const ChildUserService&) = delete;
  ChildUserService& operator=(const ChildUserService&) = delete;
  ~ChildUserService() override;

  // Returns whether web time limit was reached for child user.
  // Always returns false if per-app times limits feature is disabled.
  bool WebTimeLimitReached() const;

  // Returns whether given |url| can be used without any time restrictions.
  // Viewing of whitelisted |url| does not count towards usage web time.
  // Always returns false if per-app times limits feature is disabled.
  bool WebTimeLimitWhitelistedURL(const GURL& url) const;

  // Returns time limit set for using the web on a given day.
  // Should only be called if |features::kPerAppTimeLimits| and
  // |features::kWebTimeLimits| features are enabled.
  base::TimeDelta GetWebTimeLimit() const;

  const AppTimeController* app_time_controller() const {
    return app_time_controller_.get();
  }

  AppTimeController* app_time_controller() {
    return app_time_controller_.get();
  }

 private:
  // KeyedService:
  void Shutdown() override;

  std::unique_ptr<AppTimeController> app_time_controller_;
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_CHILD_ACCOUNTS_CHILD_USER_SERVICE_H_
