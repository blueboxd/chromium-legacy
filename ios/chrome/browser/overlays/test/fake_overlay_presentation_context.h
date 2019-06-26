// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_OVERLAYS_TEST_FAKE_OVERLAY_PRESENTATION_CONTEXT_H_
#define IOS_CHROME_BROWSER_OVERLAYS_TEST_FAKE_OVERLAY_PRESENTATION_CONTEXT_H_

#include <map>

#include "ios/chrome/browser/overlays/public/overlay_presentation_context.h"
#include "ios/chrome/browser/overlays/public/overlay_request.h"

// Fake implementation of OverlayUIDelegate used for testing.
class FakeOverlayPresentationContext : public OverlayPresentationContext {
 public:
  FakeOverlayPresentationContext();
  ~FakeOverlayPresentationContext() override;

  // Enum describing the state of the overlay UI.
  enum class PresentationState {
    // Default state.  No overlays have been presented.
    kNotPresented,
    // An overlay is currently being presented.
    kPresented,
    // A presented overlay was dismissed by user interaction.
    kUserDismissed,
    // A presented overlay was hidden.
    kHidden,
    // A presented overlay was cancelled.
    kCancelled,
  };
  // Returns the presentation state for the overlay UI.
  PresentationState GetPresentationState(OverlayRequest* request);

  // Simulates the dismissal of overlay UI for |reason|.
  void SimulateDismissalForRequest(OverlayRequest* request,
                                   OverlayDismissalReason reason);

  // OverlayUIDelegate:
  void ShowOverlayUI(OverlayPresenter* presenter,
                     OverlayRequest* request,
                     OverlayDismissalCallback dismissal_callback) override;
  void HideOverlayUI(OverlayPresenter* presenter,
                     OverlayRequest* request) override;
  void CancelOverlayUI(OverlayPresenter* presenter,
                       OverlayRequest* request) override;

 private:
  // The presentation states for each OverlayRequest.
  std::map<OverlayRequest*, PresentationState> presentation_states_;
  // The callbacks for each OverlayRequest.
  std::map<OverlayRequest*, OverlayDismissalCallback> overlay_callbacks_;
};

#endif  // IOS_CHROME_BROWSER_OVERLAYS_TEST_FAKE_OVERLAY_PRESENTATION_CONTEXT_H_
