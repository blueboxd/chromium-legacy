// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/infobars/overlays/infobar_modal_overlay_request_cancel_handler.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

#pragma mark - InfobarModalOverlayRequestCancelHandler

InfobarModalOverlayRequestCancelHandler::
    InfobarModalOverlayRequestCancelHandler(
        OverlayRequest* request,
        OverlayRequestQueue* queue,
        InfoBarIOS* infobar,
        InfobarModalCompletionNotifier* modal_completion_notifier)
    : InfobarOverlayRequestCancelHandler(request, queue, infobar),
      modal_completion_observer_(this, modal_completion_notifier, infobar) {}

InfobarModalOverlayRequestCancelHandler::
    ~InfobarModalOverlayRequestCancelHandler() = default;

#pragma mark Private

void InfobarModalOverlayRequestCancelHandler::CancelForModalCompletion() {
  CancelRequest();
}

#pragma mark - InfobarModalOverlayRequestCancelHandler::ModalCompletionObserver

InfobarModalOverlayRequestCancelHandler::ModalCompletionObserver::
    ModalCompletionObserver(
        InfobarModalOverlayRequestCancelHandler* cancel_handler,
        InfobarModalCompletionNotifier* completion_notifier,
        InfoBarIOS* infobar)
    : cancel_handler_(cancel_handler),
      infobar_(infobar),
      scoped_observer_(this) {
  DCHECK(cancel_handler_);
  DCHECK(infobar_);
  DCHECK(completion_notifier);
  scoped_observer_.Add(completion_notifier);
}

InfobarModalOverlayRequestCancelHandler::ModalCompletionObserver::
    ~ModalCompletionObserver() = default;

void InfobarModalOverlayRequestCancelHandler::ModalCompletionObserver::
    InfobarModalsCompleted(InfobarModalCompletionNotifier* notifier,
                           InfoBarIOS* infobar) {
  if (infobar_ == infobar) {
    cancel_handler_->CancelForModalCompletion();
    // The cancel handler is destroyed after CancelForModalCompletion(), so no
    // code can be added after this call.
  }
}

void InfobarModalOverlayRequestCancelHandler::ModalCompletionObserver::
    InfobarModalCompletionNotifierDestroyed(
        InfobarModalCompletionNotifier* notifier) {
  scoped_observer_.Remove(notifier);
  cancel_handler_->CancelForModalCompletion();
  // The cancel handler is destroyed after CancelForModalCompletion(), so no
  // code can be added after this call.
}
