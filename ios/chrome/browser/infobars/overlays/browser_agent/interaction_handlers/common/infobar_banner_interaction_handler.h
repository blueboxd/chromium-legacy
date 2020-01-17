// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_INFOBARS_OVERLAYS_BROWSER_AGENT_INTERACTION_HANDLERS_COMMON_INFOBAR_BANNER_INTERACTION_HANDLER_H_
#define IOS_CHROME_BROWSER_INFOBARS_OVERLAYS_BROWSER_AGENT_INTERACTION_HANDLERS_COMMON_INFOBAR_BANNER_INTERACTION_HANDLER_H_

#include "base/memory/weak_ptr.h"
#import "ios/chrome/browser/infobars/overlays/browser_agent/interaction_handlers/infobar_interaction_handler.h"

class OverlayRequestSupport;
namespace web {
class WebState;
}

// A InfobarInteractionHandler::InteractionHandler, intended to be subclassed,
// that handles interaction events for an infobar banner.
class InfobarBannerInteractionHandler
    : public InfobarInteractionHandler::Handler {
 public:
  ~InfobarBannerInteractionHandler() override;

  // Updates the model when the visibility of |infobar|'s banner is changed.
  virtual void BannerVisibilityChanged(InfoBarIOS* infobar, bool visible) = 0;
  // Updates the model when the main button is tapped for |infobar|'s banner.
  virtual void MainButtonTapped(InfoBarIOS* infobar) = 0;
  // Shows the modal when the modal button is tapped for |infobar|'s banner.
  // |web_state| is the WebState associated with |infobar|'s InfoBarManager.
  virtual void ShowModalButtonTapped(InfoBarIOS* infobar,
                                     web::WebState* web_state) = 0;
  // Notifies the model that the upcoming dismissal is user-initiated (i.e.
  // swipe dismissal in the refresh UI).
  virtual void BannerDismissedByUser(InfoBarIOS* infobar) = 0;

 protected:
  // Constructor for a banner interaction handler that creates callback
  // installers with |request_support|.
  explicit InfobarBannerInteractionHandler(
      const OverlayRequestSupport* request_support);

  // InfobarInteractionHandler::Handler:
  std::unique_ptr<OverlayRequestCallbackInstaller> CreateInstaller() override;
  void InfobarVisibilityChanged(InfoBarIOS* infobar, bool visible) override;

 private:
  // The request support passed on initialization.  Only interactions with
  // supported requests should be handled by this instance.
  const OverlayRequestSupport* request_support_ = nullptr;
};

#endif  // IOS_CHROME_BROWSER_INFOBARS_OVERLAYS_BROWSER_AGENT_INTERACTION_HANDLERS_COMMON_INFOBAR_BANNER_INTERACTION_HANDLER_H_
