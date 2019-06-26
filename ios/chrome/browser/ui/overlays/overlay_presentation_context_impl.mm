// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/overlays/overlay_presentation_context_impl.h"

#include "base/bind.h"
#include "base/callback.h"
#import "ios/chrome/browser/main/browser.h"
#import "ios/chrome/browser/overlays/public/overlay_presentation_context_observer.h"
#import "ios/chrome/browser/overlays/public/overlay_presenter.h"
#import "ios/chrome/browser/ui/overlays/overlay_container_coordinator.h"
#import "ios/chrome/browser/ui/overlays/overlay_coordinator_factory.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

#pragma mark - OverlayPresentationContextImpl::Container

OVERLAY_USER_DATA_SETUP_IMPL(OverlayPresentationContextImpl::Container);

OverlayPresentationContextImpl::Container::Container(Browser* browser)
    : browser_(browser) {
  DCHECK(browser_);
}

OverlayPresentationContextImpl::Container::~Container() = default;

OverlayPresentationContextImpl*
OverlayPresentationContextImpl::Container::PresentationContextForModality(
    OverlayModality modality) {
  auto& ui_delegate = ui_delegates_[modality];
  if (!ui_delegate) {
    ui_delegate = base::WrapUnique(
        new OverlayPresentationContextImpl(browser_, modality));
  }
  return ui_delegate.get();
}

#pragma mark - OverlayPresentationContextImpl

OverlayPresentationContextImpl::OverlayPresentationContextImpl(
    Browser* browser,
    OverlayModality modality)
    : presenter_(OverlayPresenter::FromBrowser(browser, modality)),
      shutdown_helper_(browser, presenter_),
      ui_dismissal_helper_(this),
      coordinator_factory_([OverlayRequestCoordinatorFactory
          factoryForBrowser:browser
                   modality:modality]),
      weak_factory_(this) {
  DCHECK(presenter_);
  DCHECK(coordinator_factory_);
  presenter_->SetPresentationContext(this);
}

OverlayPresentationContextImpl::~OverlayPresentationContextImpl() = default;

#pragma mark Public

void OverlayPresentationContextImpl::SetCoordinator(
    OverlayContainerCoordinator* coordinator) {
  if (coordinator_ == coordinator)
    return;

  for (auto& observer : observers_) {
    observer.OverlayPresentationContextWillChangeActivationState(this,
                                                                 !!coordinator);
  }

  coordinator_ = coordinator;

  // The new coordinator should be started before provided to the UI delegate.
  DCHECK(!coordinator_ || coordinator_.viewController);

  for (auto& observer : observers_) {
    observer.OverlayPresentationContextDidChangeActivationState(this);
  }
}

#pragma mark OverlayPresentationContext

void OverlayPresentationContextImpl::AddObserver(
    OverlayPresentationContextObserver* observer) {
  observers_.AddObserver(observer);
}

void OverlayPresentationContextImpl::RemoveObserver(
    OverlayPresentationContextObserver* observer) {
  observers_.RemoveObserver(observer);
}

bool OverlayPresentationContextImpl::IsActive() const {
  return !!coordinator_;
}

void OverlayPresentationContextImpl::ShowOverlayUI(
    OverlayPresenter* presenter,
    OverlayRequest* request,
    OverlayDismissalCallback dismissal_callback) {
  DCHECK_EQ(presenter_, presenter);
  // Create the UI state for |request| if necessary.
  if (!GetRequestUIState(request))
    states_[request] = std::make_unique<OverlayRequestUIState>(request);
  // Present the overlay UI and update the UI state.
  GetRequestUIState(request)->OverlayPresentionRequested(
      std::move(dismissal_callback));
  SetRequest(request);
}

void OverlayPresentationContextImpl::HideOverlayUI(OverlayPresenter* presenter,
                                                   OverlayRequest* request) {
  DCHECK_EQ(presenter_, presenter);
  DCHECK_EQ(request_, request);

  OverlayRequestUIState* state = GetRequestUIState(request_);
  DCHECK(state->has_callback());

  if (coordinator_) {
    // If the request's UI is presented by the coordinator, dismiss it.  The
    // presented request will be reset when the dismissal animation finishes.
    DismissPresentedUI(OverlayDismissalReason::kHiding);
  } else {
    // Simulate dismissal if there is no container coordinator.
    state->set_dismissal_reason(OverlayDismissalReason::kHiding);
    OverlayUIWasDismissed();
  }
}

void OverlayPresentationContextImpl::CancelOverlayUI(
    OverlayPresenter* presenter,
    OverlayRequest* request) {
  DCHECK_EQ(presenter_, presenter);
  // If the coordinator is not presenting the overlay UI for |state|, it can
  // be deleted immediately.
  OverlayRequestUIState* state = GetRequestUIState(request);
  DCHECK(state);
  if (!state->has_callback()) {
    states_.erase(request);
    return;
  }

  // If the current request is being cancelled (e.g. for WebState closures) when
  // there is no coordinator, simulate a dismissal.
  if (!coordinator_) {
    DCHECK_EQ(request_, request);
    state->set_dismissal_reason(OverlayDismissalReason::kCancellation);
    state->OverlayUIWasDismissed();
    return;
  }

  DismissPresentedUI(OverlayDismissalReason::kCancellation);
}

#pragma mark Accesors

void OverlayPresentationContextImpl::SetRequest(OverlayRequest* request) {
  if (request_ == request)
    return;
  if (request_) {
    OverlayRequestUIState* state = GetRequestUIState(request_);
    // The presented request should only be reset when the previously presented
    // request's UI has finished being dismissed.
    DCHECK(state);
    DCHECK(!state->has_callback());
    DCHECK(!state->coordinator().viewController.view.superview);
    // If the overlay was dismissed for user interaction or cancellation, then
    // the state can be destroyed, since the UI for the previously presented
    // request will never be shown again.
    OverlayDismissalReason reason = state->dismissal_reason();
    if (reason == OverlayDismissalReason::kUserInteraction ||
        reason == OverlayDismissalReason::kCancellation) {
      states_.erase(request_);
    }
  }

  request_ = request;

  // The UI state should be created before resetting the presented request.
  DCHECK(!request_ || GetRequestUIState(request_));

  ShowUIForPresentedRequest();
}

OverlayRequestUIState* OverlayPresentationContextImpl::GetRequestUIState(
    OverlayRequest* request) {
  return request ? states_[request].get() : nullptr;
}

#pragma mark Presentation and Dismissal helpers

void OverlayPresentationContextImpl::ShowUIForPresentedRequest() {
  OverlayRequestUIState* state = GetRequestUIState(request_);
  if (!state || !coordinator_)
    return;

  // Create the coordinator if necessary.
  UIViewController* container_view_controller = coordinator_.viewController;
  OverlayRequestCoordinator* overlay_coordinator = state->coordinator();
  if (!overlay_coordinator ||
      overlay_coordinator.baseViewController != container_view_controller) {
    overlay_coordinator = [coordinator_factory_
        newCoordinatorForRequest:request_
               dismissalDelegate:&ui_dismissal_helper_
              baseViewController:container_view_controller];
    state->OverlayUIWillBePresented(overlay_coordinator);
  }

  [overlay_coordinator startAnimated:!state->has_ui_been_presented()];
  state->OverlayUIWasPresented();
}

void OverlayPresentationContextImpl::DismissPresentedUI(
    OverlayDismissalReason reason) {
  OverlayRequestUIState* state = GetRequestUIState(request_);
  DCHECK(state);
  DCHECK(coordinator_);
  DCHECK(state->coordinator());

  state->set_dismissal_reason(reason);
  [state->coordinator()
      stopAnimated:reason == OverlayDismissalReason::kUserInteraction];
}

void OverlayPresentationContextImpl::OverlayUIWasDismissed() {
  DCHECK(request_);
  DCHECK(GetRequestUIState(request_)->has_callback());
  // If there is another request in the active WebState's OverlayRequestQueue,
  // executing the state's dismissal callback will trigger the presentation of
  // the next request.  If the presented request remains unchanged after calling
  // the dismissal callback, reset it to nullptr since the UI is no longer
  // presented.
  OverlayRequest* previously_presented_request = request_;
  GetRequestUIState(request_)->OverlayUIWasDismissed();
  if (request_ == previously_presented_request)
    SetRequest(nullptr);
}

#pragma mark BrowserShutdownHelper

OverlayPresentationContextImpl::BrowserShutdownHelper::BrowserShutdownHelper(
    Browser* browser,
    OverlayPresenter* presenter)
    : presenter_(presenter) {
  DCHECK(presenter_);
  browser->AddObserver(this);
}

OverlayPresentationContextImpl::BrowserShutdownHelper::
    ~BrowserShutdownHelper() = default;

void OverlayPresentationContextImpl::BrowserShutdownHelper::BrowserDestroyed(
    Browser* browser) {
  presenter_->SetPresentationContext(nullptr);
  browser->RemoveObserver(this);
}

#pragma mark OverlayDismissalHelper

OverlayPresentationContextImpl::OverlayDismissalHelper::OverlayDismissalHelper(
    OverlayPresentationContextImpl* presentation_context)
    : presentation_context_(presentation_context) {
  DCHECK(presentation_context_);
}

OverlayPresentationContextImpl::OverlayDismissalHelper::
    ~OverlayDismissalHelper() = default;

void OverlayPresentationContextImpl::OverlayDismissalHelper::
    OverlayUIDidFinishDismissal(OverlayRequest* request) {
  DCHECK(request);
  DCHECK_EQ(presentation_context_->request_, request);
  presentation_context_->OverlayUIWasDismissed();
}
