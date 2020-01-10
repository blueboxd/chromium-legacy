// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_INFOBARS_OVERLAYS_BROWSER_AGENT_INFOBAR_OVERLAY_BROWSER_AGENT_H_
#define IOS_CHROME_BROWSER_INFOBARS_OVERLAYS_BROWSER_AGENT_INFOBAR_OVERLAY_BROWSER_AGENT_H_

#include <map>
#include <memory>

#include "base/scoped_observer.h"
#import "ios/chrome/browser/infobars/infobar_type.h"
#import "ios/chrome/browser/main/browser_user_data.h"
#import "ios/chrome/browser/overlays/public/overlay_browser_agent_base.h"
#include "ios/chrome/browser/overlays/public/overlay_presenter.h"
#include "ios/chrome/browser/overlays/public/overlay_presenter_observer.h"

class InfobarInteractionHandler;

// Browser agent class that handles the model-layer updates for infobars.
class InfobarOverlayBrowserAgent
    : public OverlayBrowserAgentBase,
      public BrowserUserData<InfobarOverlayBrowserAgent> {
 public:
  ~InfobarOverlayBrowserAgent() override;

  // Sets the InfobarInteractionHandler to make model-layer updates for
  // interactions with infobars with |type|.  OverlayCallbackInstallers will be
  // created to forward interaction events to each interaction handler.
  // |interaction_handler| must not be null.  Must only be set once for each
  // InfobarType.
  void SetInfobarInteractionHandler(
      InfobarType type,
      std::unique_ptr<InfobarInteractionHandler> interaction_handler);

 private:
  // Constructor used by CreateForBrowser().
  friend class BrowserUserData<InfobarOverlayBrowserAgent>;
  explicit InfobarOverlayBrowserAgent(Browser* browser);
  BROWSER_USER_DATA_KEY_DECL();

  // Returns the interaction handler for the InfobarType of the infobar used to
  // configure |request|.
  InfobarInteractionHandler* GetInteractionHandler(OverlayRequest* request);

  // Helper object that notifies interaction handler of changes in banner
  // visibility.
  class BannerVisibilityObserver : public OverlayPresenterObserver {
   public:
    BannerVisibilityObserver(Browser* browser,
                             InfobarOverlayBrowserAgent* browser_agent);
    ~BannerVisibilityObserver() override;

   private:
    // Notifies the BrowserAgent's interaction handler that the visibility of
    // |request|'s banner UI has changed.
    void BannerVisibilityChanged(OverlayRequest* request, bool visible);

    // OverlayPresenterObserver:
    const OverlayRequestSupport* GetRequestSupport(
        OverlayPresenter* presenter) const override;
    void DidShowOverlay(OverlayPresenter* presenter,
                        OverlayRequest* request) override;
    void DidHideOverlay(OverlayPresenter* presenter,
                        OverlayRequest* request) override;
    void OverlayPresenterDestroyed(OverlayPresenter* presenter) override;

    InfobarOverlayBrowserAgent* browser_agent_ = nullptr;
    ScopedObserver<OverlayPresenter, OverlayPresenterObserver> scoped_observer_;
  };

  // The interaction handlers for each InfobarType.
  std::map<InfobarType, std::unique_ptr<InfobarInteractionHandler>>
      interaction_handlers_;
  // The observer for infobar banner presentations and dismissals.
  BannerVisibilityObserver banner_visibility_observer_;
};

#endif  // IOS_CHROME_BROWSER_INFOBARS_OVERLAYS_BROWSER_AGENT_INFOBAR_OVERLAY_BROWSER_AGENT_H_
