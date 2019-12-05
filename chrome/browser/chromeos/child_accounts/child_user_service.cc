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

ChildUserService::TestApi::TestApi(ChildUserService* service)
    : service_(service) {}

ChildUserService::TestApi::~TestApi() = default;

app_time::AppTimeController* ChildUserService::TestApi::app_time_controller() {
  return service_->app_time_controller_.get();
}

app_time::WebTimeLimitEnforcer* ChildUserService::TestApi::web_time_enforcer() {
  return app_time_controller()
             ? service_->app_time_controller_->web_time_enforcer()
             : nullptr;
}

ChildUserService::ChildUserService(content::BrowserContext* context) {
  if (app_time::AppTimeController::ArePerAppTimeLimitsEnabled())
    app_time_controller_ = std::make_unique<app_time::AppTimeController>();
}

ChildUserService::~ChildUserService() = default;

void ChildUserService::PauseWebActivity() {
  DCHECK(app_time_controller_);

  app_time::WebTimeLimitEnforcer* web_time_enforcer =
      app_time_controller_->web_time_enforcer();
  DCHECK(web_time_enforcer);

  // TODO(agawronska): Pass the time limit to |web_time_enforcer|.
  web_time_enforcer->OnWebTimeLimitReached();
}

void ChildUserService::ResumeWebActivity() {
  DCHECK(app_time_controller_);

  app_time::WebTimeLimitEnforcer* web_time_enforcer =
      app_time_controller_->web_time_enforcer();
  DCHECK(web_time_enforcer);

  web_time_enforcer->set_time_limit(base::TimeDelta());
  web_time_enforcer->OnWebTimeLimitEnded();
}

bool ChildUserService::WebTimeLimitReached() const {
  if (!app_time_controller_)
    return false;
  DCHECK(app_time_controller_->web_time_enforcer());
  return app_time_controller_->web_time_enforcer()->blocked();
}

bool ChildUserService::WebTimeLimitWhitelistedURL(const GURL& url) const {
  if (!app_time_controller_)
    return false;
  DCHECK(app_time_controller_->web_time_enforcer());
  return app_time_controller_->web_time_enforcer()->IsURLWhitelisted(url);
}

base::TimeDelta ChildUserService::GetWebTimeLimit() const {
  DCHECK(app_time_controller_);
  DCHECK(app_time_controller_->web_time_enforcer());
  return app_time_controller_->web_time_enforcer()->time_limit();
}

void ChildUserService::Shutdown() {
  app_time_controller_.reset();
}

}  // namespace chromeos
