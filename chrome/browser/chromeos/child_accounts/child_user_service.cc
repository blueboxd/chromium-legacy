// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/child_accounts/child_user_service.h"

#include "base/time/time.h"
#include "chrome/browser/chromeos/child_accounts/time_limits/app_time_controller.h"
#include "chrome/browser/chromeos/child_accounts/time_limits/web_time_limit_enforcer.h"
#include "content/public/browser/browser_context.h"
#include "url/gurl.h"

namespace chromeos {

ChildUserService::ChildUserService(content::BrowserContext* context) {
  if (AppTimeController::ArePerAppTimeLimitsEnabled())
    app_time_controller_ = std::make_unique<AppTimeController>();
}

ChildUserService::~ChildUserService() = default;

bool ChildUserService::WebTimeLimitReached() const {
  if (!app_time_controller())
    return false;
  DCHECK(app_time_controller()->web_time_enforcer());
  return app_time_controller()->web_time_enforcer()->blocked();
}

bool ChildUserService::WebTimeLimitWhitelistedURL(const GURL& url) const {
  if (!app_time_controller())
    return false;
  DCHECK(app_time_controller()->web_time_enforcer());
  return app_time_controller()->web_time_enforcer()->IsURLWhitelisted(url);
}

base::TimeDelta ChildUserService::GetWebTimeLimit() const {
  DCHECK(app_time_controller());
  DCHECK(app_time_controller()->web_time_enforcer());
  return app_time_controller()->web_time_enforcer()->time_limit();
}

void ChildUserService::Shutdown() {
  app_time_controller_.reset();
}

}  // namespace chromeos
