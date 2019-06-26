// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ios/chrome/browser/overlays/test/fake_overlay_presentation_context.h"

#include "base/bind.h"
#include "base/logging.h"
#include "ios/chrome/browser/overlays/public/overlay_request_queue.h"

FakeOverlayPresentationContext::FakeOverlayPresentationContext() = default;
FakeOverlayPresentationContext::~FakeOverlayPresentationContext() = default;

FakeOverlayPresentationContext::PresentationState
FakeOverlayPresentationContext::GetPresentationState(OverlayRequest* request) {
  return presentation_states_[request];
}

void FakeOverlayPresentationContext::SimulateDismissalForRequest(
    OverlayRequest* request,
    OverlayDismissalReason reason) {
  DCHECK_EQ(PresentationState::kPresented, presentation_states_[request]);
  switch (reason) {
    case OverlayDismissalReason::kUserInteraction:
      presentation_states_[request] = PresentationState::kUserDismissed;
      break;
    case OverlayDismissalReason::kHiding:
      presentation_states_[request] = PresentationState::kHidden;
      break;
    case OverlayDismissalReason::kCancellation:
      presentation_states_[request] = PresentationState::kCancelled;
      break;
  }
  std::move(overlay_callbacks_[request]).Run(reason);
}

void FakeOverlayPresentationContext::ShowOverlayUI(
    OverlayPresenter* presenter,
    OverlayRequest* request,
    OverlayDismissalCallback dismissal_callback) {
  presentation_states_[request] = PresentationState::kPresented;
  overlay_callbacks_[request] = std::move(dismissal_callback);
}

void FakeOverlayPresentationContext::HideOverlayUI(OverlayPresenter* presenter,
                                                   OverlayRequest* request) {
  SimulateDismissalForRequest(request, OverlayDismissalReason::kHiding);
}

void FakeOverlayPresentationContext::CancelOverlayUI(
    OverlayPresenter* presenter,
    OverlayRequest* request) {
  PresentationState& state = presentation_states_[request];
  if (state == PresentationState::kPresented) {
    SimulateDismissalForRequest(request, OverlayDismissalReason::kCancellation);
  } else {
    state = PresentationState::kCancelled;
  }
}
