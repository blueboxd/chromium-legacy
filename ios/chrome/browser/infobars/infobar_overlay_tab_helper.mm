// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/infobars/infobar_overlay_tab_helper.h"

#include "base/logging.h"
#import "ios/chrome/browser/infobars/infobar_banner_overlay_request_factory.h"
#include "ios/chrome/browser/infobars/infobar_manager_impl.h"
#include "ios/chrome/browser/overlays/public/overlay_request.h"
#import "ios/chrome/browser/overlays/public/overlay_request_queue.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

using infobars::InfoBar;
using infobars::InfoBarManager;

#pragma mark - InfobarOverlayTabHelper

WEB_STATE_USER_DATA_KEY_IMPL(InfobarOverlayTabHelper)

// static
void InfobarOverlayTabHelper::CreateForWebState(
    web::WebState* web_state,
    std::unique_ptr<InfobarBannerOverlayRequestFactory> request_factory) {
  DCHECK(web_state);
  if (!FromWebState(web_state)) {
    web_state->SetUserData(UserDataKey(),
                           base::WrapUnique(new InfobarOverlayTabHelper(
                               web_state, std::move(request_factory))));
  }
}

InfobarOverlayTabHelper::InfobarOverlayTabHelper(
    web::WebState* web_state,
    std::unique_ptr<InfobarBannerOverlayRequestFactory> request_factory)
    : overlay_request_scheduler_(web_state, std::move(request_factory)) {}

InfobarOverlayTabHelper::~InfobarOverlayTabHelper() = default;

#pragma mark - InfobarOverlayTabHelper::OverlayRequestScheduler

InfobarOverlayTabHelper::OverlayRequestScheduler::OverlayRequestScheduler(
    web::WebState* web_state,
    std::unique_ptr<InfobarBannerOverlayRequestFactory> request_factory)
    : queue_(
          OverlayRequestQueue::FromWebState(web_state,
                                            OverlayModality::kInfobarBanner)),
      request_factory_(std::move(request_factory)),
      scoped_observer_(this) {
  DCHECK(web_state);
  DCHECK(queue_);
  DCHECK(request_factory_);
  InfoBarManager* manager = InfoBarManagerImpl::FromWebState(web_state);
  DCHECK(manager);
  scoped_observer_.Add(manager);
}

InfobarOverlayTabHelper::OverlayRequestScheduler::~OverlayRequestScheduler() =
    default;

void InfobarOverlayTabHelper::OverlayRequestScheduler::OnInfoBarAdded(
    InfoBar* infobar) {
  std::unique_ptr<OverlayRequest> request =
      request_factory_->CreateBannerRequest(infobar);
  DCHECK(request);
  queue_->AddRequest(std::move(request));
}

void InfobarOverlayTabHelper::OverlayRequestScheduler::OnManagerShuttingDown(
    InfoBarManager* manager) {
  scoped_observer_.Remove(manager);
}
