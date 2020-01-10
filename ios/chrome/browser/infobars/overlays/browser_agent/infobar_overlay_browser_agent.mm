// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/infobars/overlays/browser_agent/infobar_overlay_browser_agent.h"

#include "base/logging.h"
#import "ios/chrome/browser/infobars/overlays/browser_agent/infobar_banner_overlay_request_callback_installer.h"
#import "ios/chrome/browser/infobars/overlays/browser_agent/interaction_handlers/infobar_interaction_handler.h"
#include "ios/chrome/browser/infobars/overlays/overlay_request_infobar_util.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

#pragma mark - InfobarOverlayBrowserAgent

BROWSER_USER_DATA_KEY_IMPL(InfobarOverlayBrowserAgent)

InfobarOverlayBrowserAgent::InfobarOverlayBrowserAgent(Browser* browser)
    : OverlayBrowserAgentBase(browser),
      banner_visibility_observer_(browser, this) {}

InfobarOverlayBrowserAgent::~InfobarOverlayBrowserAgent() = default;

#pragma mark Public

void InfobarOverlayBrowserAgent::SetInfobarInteractionHandler(
    InfobarType type,
    std::unique_ptr<InfobarInteractionHandler> interaction_handler) {
  // Only one installer should be set for a single request type.  Otherwise, the
  // previously-set handler will be destroyed and callbacks forwarded to it will
  // crash.
  DCHECK(!interaction_handlers_[type]);
  // Create callback installers for each supported handler.
  const OverlayRequestSupport* support = interaction_handler->request_support();
  AddInstaller(std::make_unique<InfobarBannerOverlayRequestCallbackInstaller>(
                   support, interaction_handler->banner_handler()),
               OverlayModality::kInfobarBanner);
  InfobarDetailSheetInteractionHandler* sheet_handler =
      interaction_handler->sheet_handler();
  if (sheet_handler) {
    // TODO(crbug.com/1030357): Install callbacks for detail sheet when
    // implemented.
  }
  InfobarModalInteractionHandler* modal_handler =
      interaction_handler->modal_handler();
  if (modal_handler) {
    // TODO(crbug.com/1030357): Install callbacks for modal when implemented.
  }
  // Add the interaction handler to the list.
  interaction_handlers_[type] = std::move(interaction_handler);
}

#pragma mark Private

InfobarInteractionHandler* InfobarOverlayBrowserAgent::GetInteractionHandler(
    OverlayRequest* request) {
  auto& interaction_handler =
      interaction_handlers_[GetOverlayRequestInfobarType(request)];
  DCHECK(interaction_handler);
  DCHECK(interaction_handler->request_support()->IsRequestSupported(request));
  return interaction_handler.get();
}

#pragma mark - InfobarOverlayBrowserAgent::BannerVisibilityObserver

InfobarOverlayBrowserAgent::BannerVisibilityObserver::BannerVisibilityObserver(
    Browser* browser,
    InfobarOverlayBrowserAgent* browser_agent)
    : browser_agent_(browser_agent), scoped_observer_(this) {
  DCHECK(browser_agent_);
  scoped_observer_.Add(
      OverlayPresenter::FromBrowser(browser, OverlayModality::kInfobarBanner));
}

InfobarOverlayBrowserAgent::BannerVisibilityObserver::
    ~BannerVisibilityObserver() = default;

void InfobarOverlayBrowserAgent::BannerVisibilityObserver::
    BannerVisibilityChanged(OverlayRequest* request, bool visible) {
  browser_agent_->GetInteractionHandler(request)
      ->banner_handler()
      ->BannerVisibilityChanged(GetOverlayRequestInfobar(request), visible);
}

const OverlayRequestSupport*
InfobarOverlayBrowserAgent::BannerVisibilityObserver::GetRequestSupport(
    OverlayPresenter* presenter) const {
  return browser_agent_->GetRequestSupport(presenter->GetModality());
}

void InfobarOverlayBrowserAgent::BannerVisibilityObserver::DidShowOverlay(
    OverlayPresenter* presenter,
    OverlayRequest* request) {
  BannerVisibilityChanged(request, /*visible=*/true);
}

void InfobarOverlayBrowserAgent::BannerVisibilityObserver::DidHideOverlay(
    OverlayPresenter* presenter,
    OverlayRequest* request) {
  BannerVisibilityChanged(request, /*visible=*/false);
}

void InfobarOverlayBrowserAgent::BannerVisibilityObserver::
    OverlayPresenterDestroyed(OverlayPresenter* presenter) {
  scoped_observer_.Remove(presenter);
}
