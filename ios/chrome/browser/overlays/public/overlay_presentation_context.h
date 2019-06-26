// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_OVERLAYS_PUBLIC_OVERLAY_PRESENTATION_CONTEXT_H_
#define IOS_CHROME_BROWSER_OVERLAYS_PUBLIC_OVERLAY_PRESENTATION_CONTEXT_H_

#include "ios/chrome/browser/overlays/public/overlay_dismissal_callback.h"

class OverlayPresenter;
class OverlayRequest;
class OverlayPresentationContextObserver;

// Object that handles presenting the overlay UI for OverlayPresenter.
class OverlayPresentationContext {
 public:
  OverlayPresentationContext() = default;
  virtual ~OverlayPresentationContext() = default;

  // Adds and removes |observer|.
  virtual void AddObserver(OverlayPresentationContextObserver* observer) = 0;
  virtual void RemoveObserver(OverlayPresentationContextObserver* observer) = 0;

  // Whether the presentation context is active.  Overlay UI will only be
  // presented for active contexts.
  virtual bool IsActive() const = 0;

  // Called by |presenter| to show the overlay UI for |request|.
  // |dismissal_callback| must be stored and called whenever the UI is finished
  // being dismissed for user interaction, hiding, or cancellation.
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

#endif  // IOS_CHROME_BROWSER_OVERLAYS_PUBLIC_OVERLAY_PRESENTATION_CONTEXT_H_
