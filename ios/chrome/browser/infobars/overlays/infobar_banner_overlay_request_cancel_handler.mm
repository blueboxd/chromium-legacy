// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/infobars/overlays/infobar_banner_overlay_request_cancel_handler.h"

#include "ios/chrome/browser/infobars/infobar_ios.h"
#import "ios/chrome/browser/infobars/overlays/infobar_overlay_request_inserter.h"
#import "ios/chrome/browser/infobars/overlays/infobar_overlay_type.h"
#include "ios/chrome/browser/infobars/overlays/overlay_request_infobar_util.h"
#import "ios/chrome/browser/overlays/public/overlay_request_queue.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

#pragma mark - InfobarBannerOverlayRequestCancelHandler

InfobarBannerOverlayRequestCancelHandler::
    InfobarBannerOverlayRequestCancelHandler(
        OverlayRequest* request,
        OverlayRequestQueue* queue,
        const InfobarOverlayRequestInserter* inserter,
        InfobarModalCompletionNotifier* modal_completion_notifier)
    : InfobarOverlayRequestCancelHandler(request, queue),
      inserter_(inserter),
      modal_completion_observer_(this, modal_completion_notifier, infobar()) {
  DCHECK(inserter_);
}

InfobarBannerOverlayRequestCancelHandler::
    ~InfobarBannerOverlayRequestCancelHandler() = default;

#pragma mark Private

void InfobarBannerOverlayRequestCancelHandler::CancelForModalCompletion() {
  CancelRequest();
}

#pragma mark InfobarOverlayRequestCancelHandler

void InfobarBannerOverlayRequestCancelHandler::HandleReplacement(
    InfoBarIOS* replacement) {
  // If an infobar is replaced while a request for its banner is in the queue,
  // a request for the replacement's banner should be inserted in back of the
  // handler's request.
  size_t index = 0;
  while (index < queue()->size()) {
    if (GetOverlayRequestInfobar(queue()->GetRequest(index)) == infobar())
      break;
    ++index;
  }
  DCHECK_LT(index, queue()->size());
  inserter_->InsertOverlayRequest(replacement, InfobarOverlayType::kBanner,
                                  index + 1);
}

#pragma mark - InfobarBannerOverlayRequestCancelHandler::ModalCompletionObserver

InfobarBannerOverlayRequestCancelHandler::ModalCompletionObserver::
    ModalCompletionObserver(
        InfobarBannerOverlayRequestCancelHandler* cancel_handler,
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

InfobarBannerOverlayRequestCancelHandler::ModalCompletionObserver::
    ~ModalCompletionObserver() = default;

void InfobarBannerOverlayRequestCancelHandler::ModalCompletionObserver::
    InfobarModalsCompleted(InfobarModalCompletionNotifier* notifier,
                           InfoBarIOS* infobar) {
  if (infobar_ == infobar) {
    cancel_handler_->CancelForModalCompletion();
    // The cancel handler is destroyed after CancelForModalCompletion(), so no
    // code can be added after this call.
  }
}

void InfobarBannerOverlayRequestCancelHandler::ModalCompletionObserver::
    InfobarModalCompletionNotifierDestroyed(
        InfobarModalCompletionNotifier* notifier) {
  scoped_observer_.Remove(notifier);
  cancel_handler_->CancelForModalCompletion();
  // The cancel handler is destroyed after CancelForModalCompletion(), so no
  // code can be added after this call.
}
