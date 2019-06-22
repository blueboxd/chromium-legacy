// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_OVERLAYS_PUBLIC_OVERLAY_PRESENTER_H_
#define IOS_CHROME_BROWSER_OVERLAYS_PUBLIC_OVERLAY_PRESENTER_H_

#include <memory>

#include "base/observer_list_types.h"
#include "ios/chrome/browser/overlays/public/overlay_dismissal_callback.h"
#include "ios/chrome/browser/overlays/public/overlay_modality.h"

class Browser;
class OverlayRequest;

// OverlayPresenter handles the presentation of overlay UI for OverlayRequests
// added to the OverlayRequestQueues for WebStates in a Browser.
class OverlayPresenter {
 public:
  virtual ~OverlayPresenter() = default;

  // Retrieves the OverlayPresenter for |browser| that manages overlays at
  // |modality|, creating one if necessary.
  static OverlayPresenter* FromBrowser(Browser* browser,
                                       OverlayModality modality);

  // Delegate that handles presenting the overlay UI for OverlayPresenter
  class UIDelegate : public base::CheckedObserver {
   public:
    UIDelegate() = default;

    // Called by |presenter| to show the overlay UI for |request|.
    // |dismissal_callback| must be stored by the delegate and called whenever
    // the UI is finished being dismissed for user interaction, hiding, or
    // cancellation.
    virtual void ShowOverlayUI(OverlayPresenter* presenter,
                               OverlayRequest* request,
                               OverlayDismissalCallback dismissal_callback) = 0;

    // Called by |presenter| to hide the overlay UI for |request|.  Hidden
    // overlays may be shown again, so they should be kept in memory or
    // serialized so that the state can be restored if shown again.  When hiding
    // an overlay, the presented UI must be dismissed, and the overlay's
    // dismissal callback must must be executed upon the dismissal's completion.
    virtual void HideOverlayUI(OverlayPresenter* presenter,
                               OverlayRequest* request) = 0;

    // Called by |presenter| to cancel the overlay UI for |request|.  If the UI
    // is presented, it should be dismissed and the dismissal callback should be
    // executed upon the dismissal's completion.  Otherwise, any state
    // corresponding to any hidden overlays should be cleaned up.
    virtual void CancelOverlayUI(OverlayPresenter* presenter,
                                 OverlayRequest* request) = 0;
  };

  // Sets the UI delegate for the presenter.  Upon being set, the presenter will
  // attempt to begin presenting overlay UI for the active WebState in its
  // Browser.
  virtual void SetUIDelegate(UIDelegate* delegate) = 0;

  // Observer interface for objects interested in overlay presentation events
  // triggered by OverlayPresenter.
  class Observer : public base::CheckedObserver {
   public:
    Observer() = default;

    // Called when |presenter| is about to show the overlay UI for |request|.
    virtual void WillShowOverlay(OverlayPresenter* presenter,
                                 OverlayRequest* request) {}

    // Called when |presenter| is finished dismissing the overlay UI for
    // |request|.
    virtual void DidHideOverlay(OverlayPresenter* presenter,
                                OverlayRequest* request) {}

    // Called when |presenter| is destroyed.
    virtual void OverlayPresenterDestroyed(OverlayPresenter* presenter) {}
  };

  // Adds and removes observers.
  virtual void AddObserver(Observer* observer) = 0;
  virtual void RemoveObserver(Observer* observer) = 0;
};

#endif  // IOS_CHROME_BROWSER_OVERLAYS_PUBLIC_OVERLAY_PRESENTER_H_
