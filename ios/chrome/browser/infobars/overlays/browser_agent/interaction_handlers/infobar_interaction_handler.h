// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_INFOBARS_OVERLAYS_BROWSER_AGENT_INTERACTION_HANDLERS_INFOBAR_INTERACTION_HANDLER_H_
#define IOS_CHROME_BROWSER_INFOBARS_OVERLAYS_BROWSER_AGENT_INTERACTION_HANDLERS_INFOBAR_INTERACTION_HANDLER_H_

#include <memory>

#include "ios/chrome/browser/overlays/public/overlay_request_support.h"

class InfoBarIOS;
namespace web {
class WebState;
}

// Handler for infobar banner user interaction events.
class InfobarBannerInteractionHandler {
 public:
  virtual ~InfobarBannerInteractionHandler() = default;

  // Updates the model when the visibility of |infobar|'s banner is changed.
  virtual void BannerVisibilityChanged(InfoBarIOS* infobar, bool visible) = 0;
  // Updates the model when the main button is tapped for |infobar|'s banner.
  virtual void MainButtonTapped(InfoBarIOS* infobar) = 0;
  // Shows the modal when the modal button is tapped for |infobar|'s banner.
  // |web_state| is the WebState associated with |infobar|'s InfoBarManager.
  virtual void ShowModalButtonTapped(InfoBarIOS* infobar,
                                     web::WebState* web_state) = 0;
  // Notifies the model that the upcoming dismissal is user-initiated (i.e.
  // a swipe dismissal in the refresh UI).
  virtual void BannerDismissedByUser(InfoBarIOS* infobar) = 0;
};

// Handler for infobar detail sheet user interaction events.
class InfobarDetailSheetInteractionHandler {
 public:
  virtual ~InfobarDetailSheetInteractionHandler() = default;
  // TODO(crbug.com/1030357): Add interaction handling for detail sheets.
};

// Handler for infobar modal user interaction events.
class InfobarModalInteractionHandler {
 public:
  virtual ~InfobarModalInteractionHandler() = default;
  // TODO(crbug.com/1030357): Add interaction handling for modals.
};

// Helper object, intended to be subclassed, that encapsulates the model-layer
// updates required for interaction with each type of UI used to display an
// infobar.  Subclasses should be created for each InfobarType to manage the
// user interaction for InfoBars of that type.
class InfobarInteractionHandler {
 public:
  virtual ~InfobarInteractionHandler();

  // Returns the request support for the handler.  Interaction events will only
  // be handled for supported requests.  Guaranteed to be non-null.
  const OverlayRequestSupport* request_support() const {
    return request_support_;
  }

  // Returns the handlers for each InfobarOverlayType.  Guaranteed to be
  // non-null.
  InfobarBannerInteractionHandler* banner_handler() const {
    return banner_handler_.get();
  }
  // Returns the detail sheet handler for this interaction handler.
  InfobarDetailSheetInteractionHandler* sheet_handler() const {
    return sheet_handler_.get();
  }
  // Returns the modal handler for this interaction handler.
  InfobarModalInteractionHandler* modal_handler() const {
    return modal_handler_.get();
  }

 protected:
  // Initializer used by subclasses that return the passed handlers from the
  // getters above.  |banner_handler| must be non-null for all InfobarTypes.
  // |sheet_handler| and |modal_handler| may be null if the infobar whose
  // interactions are being handled do not support these overlay types.
  InfobarInteractionHandler(
      const OverlayRequestSupport* request_support,
      std::unique_ptr<InfobarBannerInteractionHandler> banner_handler,
      std::unique_ptr<InfobarDetailSheetInteractionHandler> sheet_handler,
      std::unique_ptr<InfobarModalInteractionHandler> modal_handler);

  // The request support passed on initialization.  Only interactions with
  // supported requests should be handled by this instance.
  const OverlayRequestSupport* request_support_ = nullptr;
  // The interaction handlers passed on initialization.
  std::unique_ptr<InfobarBannerInteractionHandler> banner_handler_;
  std::unique_ptr<InfobarDetailSheetInteractionHandler> sheet_handler_;
  std::unique_ptr<InfobarModalInteractionHandler> modal_handler_;
};

#endif  // IOS_CHROME_BROWSER_INFOBARS_OVERLAYS_BROWSER_AGENT_INTERACTION_HANDLERS_INFOBAR_INTERACTION_HANDLER_H_
