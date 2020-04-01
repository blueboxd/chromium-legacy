// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ios/chrome/browser/interstitials/ios_chrome_controller_client.h"

#include "base/bind.h"
#include "base/logging.h"
#include "base/task/post_task.h"
#include "components/security_interstitials/core/metrics_helper.h"
#include "ios/chrome/browser/application_context.h"
#include "ios/chrome/browser/browser_state/chrome_browser_state.h"
#include "ios/chrome/browser/pref_names.h"
#import "ios/web/public/navigation/navigation_manager.h"
#include "ios/web/public/navigation/reload_type.h"
#include "ios/web/public/security/web_interstitial.h"
#include "ios/web/public/thread/web_task_traits.h"
#include "ios/web/public/thread/web_thread.h"
#import "ios/web/public/web_state.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

IOSChromeControllerClient::IOSChromeControllerClient(
    web::WebState* web_state,
    std::unique_ptr<security_interstitials::MetricsHelper> metrics_helper)
    : security_interstitials::ControllerClient(std::move(metrics_helper)),
      web_state_(web_state),
      web_interstitial_(nullptr),
      weak_factory_(this) {
  web_state_->AddObserver(this);
}

IOSChromeControllerClient::~IOSChromeControllerClient() {
  if (web_state_) {
    web_state_->RemoveObserver(this);
  }
}

void IOSChromeControllerClient::SetWebInterstitial(
    web::WebInterstitial* web_interstitial) {
  web_interstitial_ = web_interstitial;
}

void IOSChromeControllerClient::WebStateDestroyed(web::WebState* web_state) {
  DCHECK_EQ(web_state_, web_state);
  web_state_->RemoveObserver(this);
  web_state_ = nullptr;
}

bool IOSChromeControllerClient::CanLaunchDateAndTimeSettings() {
  return false;
}

void IOSChromeControllerClient::LaunchDateAndTimeSettings() {
  NOTREACHED();
}

void IOSChromeControllerClient::GoBack() {
  if (CanGoBack()) {
    web_state_->GetNavigationManager()->GoBack();
  } else {
    // Closing the tab synchronously is problematic since web state is heavily
    // involved in the operation and CloseWebState interrupts it, so call
    // CloseWebState asynchronously.
    base::PostTask(FROM_HERE, {web::WebThread::UI},
                   base::BindOnce(&IOSChromeControllerClient::Close,
                                  weak_factory_.GetWeakPtr()));
  }
}

bool IOSChromeControllerClient::CanGoBack() {
  return web_state_->GetNavigationManager()->CanGoBack();
}

bool IOSChromeControllerClient::CanGoBackBeforeNavigation() {
  NOTREACHED();
  return false;
}

void IOSChromeControllerClient::GoBackAfterNavigationCommitted() {
  NOTREACHED();
}

void IOSChromeControllerClient::Proceed() {
  DCHECK(web_interstitial_);
  web_interstitial_->Proceed();
}

void IOSChromeControllerClient::Reload() {
  web_state_->GetNavigationManager()->Reload(web::ReloadType::NORMAL,
                                             true /*check_for_repost*/);
}

void IOSChromeControllerClient::OpenUrlInCurrentTab(const GURL& url) {
  web_state_->OpenURL(web::WebState::OpenURLParams(
      url, web::Referrer(), WindowOpenDisposition::CURRENT_TAB,
      ui::PAGE_TRANSITION_LINK, false));
}

void IOSChromeControllerClient::OpenUrlInNewForegroundTab(const GURL& url) {
  web_state_->OpenURL(web::WebState::OpenURLParams(
      url, web::Referrer(), WindowOpenDisposition::NEW_FOREGROUND_TAB,
      ui::PAGE_TRANSITION_LINK, false));
}

const std::string& IOSChromeControllerClient::GetApplicationLocale() const {
  return GetApplicationContext()->GetApplicationLocale();
}

PrefService* IOSChromeControllerClient::GetPrefService() {
  return ChromeBrowserState::FromBrowserState(web_state_->GetBrowserState())
      ->GetPrefs();
}

const std::string IOSChromeControllerClient::GetExtendedReportingPrefName()
    const {
  return std::string();
}

void IOSChromeControllerClient::Close() {
  if (web_state_) {
    web_state_->CloseWebState();
  }
}
