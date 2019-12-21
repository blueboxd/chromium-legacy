// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/infobars/infobar_badge_browser_agent.h"

#include "base/logging.h"
#include "ios/chrome/browser/infobars/infobar_badge_tab_helper.h"
#import "ios/chrome/browser/overlays/public/common/infobars/infobar_overlay_request_config.h"
#include "ios/chrome/browser/overlays/public/overlay_callback_manager.h"
#include "ios/chrome/browser/overlays/public/overlay_presenter.h"
#include "ios/chrome/browser/overlays/public/overlay_request.h"
#import "ios/chrome/browser/web_state_list/web_state_list.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace {
// Returns the InfobarType used to configure |request|.  |request| must be
// configured with an InfobarOverlayRequestConfig.
InfobarType GetInfobarType(OverlayRequest* request) {
  return request->GetConfig<InfobarOverlayRequestConfig>()->infobar_type();
}
}  // namespace

#pragma mark - InfobarBadgeBrowserAgent

BROWSER_USER_DATA_KEY_IMPL(InfobarBadgeBrowserAgent)

InfobarBadgeBrowserAgent::InfobarBadgeBrowserAgent(Browser* browser)
    : infobar_overlay_observer_(this, browser),
      web_state_list_(browser->GetWebStateList()) {
  DCHECK(web_state_list_);
}

InfobarBadgeBrowserAgent::~InfobarBadgeBrowserAgent() = default;

InfobarBadgeTabHelper* InfobarBadgeBrowserAgent::GetBadgeCurrentTabHelper()
    const {
  if (web_state_list_->active_index() == WebStateList::kInvalidIndex)
    return nullptr;
  return InfobarBadgeTabHelper::FromWebState(
      web_state_list_->GetActiveWebState());
}

void InfobarBadgeBrowserAgent::OnInfobarBannerPresented(
    OverlayRequest* request) {
  InfobarBadgeTabHelper* tab_helper = GetBadgeCurrentTabHelper();
  if (tab_helper)
    tab_helper->UpdateBadgeForInfobarBannerPresented(GetInfobarType(request));
}

void InfobarBadgeBrowserAgent::OnInfobarBannerDismissed(
    OverlayRequest* request) {
  InfobarBadgeTabHelper* tab_helper = GetBadgeCurrentTabHelper();
  if (tab_helper)
    tab_helper->UpdateBadgeForInfobarBannerDismissed(GetInfobarType(request));
}

#pragma mark - InfobarBadgeBrowserAgent::InfobarOverlayObserver

InfobarBadgeBrowserAgent::InfobarOverlayObserver::InfobarOverlayObserver(
    InfobarBadgeBrowserAgent* browser_agent,
    Browser* browser)
    : scoped_observer_(this), browser_agent_(browser_agent) {
  DCHECK(browser_agent_);
  scoped_observer_.Add(
      OverlayPresenter::FromBrowser(browser, OverlayModality::kInfobarBanner));
  scoped_observer_.Add(
      OverlayPresenter::FromBrowser(browser, OverlayModality::kInfobarModal));
}

InfobarBadgeBrowserAgent::InfobarOverlayObserver::~InfobarOverlayObserver() =
    default;

void InfobarBadgeBrowserAgent::InfobarOverlayObserver::DidShowOverlay(
    OverlayPresenter* presenter,
    OverlayRequest* request) {
  browser_agent_->OnInfobarBannerPresented(request);
}

void InfobarBadgeBrowserAgent::InfobarOverlayObserver::DidHideOverlay(
    OverlayPresenter* presenter,
    OverlayRequest* request) {
  browser_agent_->OnInfobarBannerDismissed(request);
}

void InfobarBadgeBrowserAgent::InfobarOverlayObserver::
    OverlayPresenterDestroyed(OverlayPresenter* presenter) {
  scoped_observer_.Remove(presenter);
}
