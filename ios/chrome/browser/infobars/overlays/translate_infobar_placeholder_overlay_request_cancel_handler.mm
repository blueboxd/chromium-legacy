// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/infobars/overlays/translate_infobar_placeholder_overlay_request_cancel_handler.h"

#import "ios/chrome/browser/infobars/infobar_type.h"
#import "ios/chrome/browser/infobars/overlays/infobar_overlay_request_inserter.h"
#include "ios/chrome/browser/infobars/overlays/overlay_request_infobar_util.h"
#import "ios/chrome/browser/overlays/public/common/placeholder_request_config.h"
#import "ios/chrome/browser/overlays/public/overlay_request_cancel_handler.h"
#import "ios/chrome/browser/overlays/public/overlay_request_queue.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace translate_infobar_overlays {

PlaceholderRequestCancelHandler::PlaceholderRequestCancelHandler(
    OverlayRequest* request,
    OverlayRequestQueue* queue,
    InfobarOverlayRequestInserter* inserter,
    InfoBarIOS* translate_infobar)
    : InfobarOverlayRequestCancelHandler(request, queue, translate_infobar),
      inserter_(inserter),
      translate_delegate_observer_(
          infobar()->delegate()->AsTranslateInfoBarDelegate(),
          this) {
  DCHECK(inserter_);
}

PlaceholderRequestCancelHandler::~PlaceholderRequestCancelHandler() = default;

void PlaceholderRequestCancelHandler::TranslateStepChanged(
    translate::TranslateStep step) {
  if (step == translate::TranslateStep::TRANSLATE_STEP_AFTER_TRANSLATE) {
    infobar()->set_accepted(true);
    size_t insert_index = 0;
    for (size_t index = 0; index < queue()->size(); index++) {
      if (queue()->GetRequest(index) == request()) {
        insert_index = index + 1;
        break;
      }
    }

    // Banner UI will be different from initial banner to show the
    // AFTER_TRANSLATE state.
    InsertParams params(infobar());
    params.overlay_type = InfobarOverlayType::kBanner;
    params.insertion_index = insert_index;
    params.source = InfobarOverlayInsertionSource::kInfoBarDelegate;
    inserter_->InsertOverlayRequest(params);
    CancelRequest();
  }
}

#pragma mark - TranslateDelegateObserver

PlaceholderRequestCancelHandler::TranslateDelegateObserver::
    TranslateDelegateObserver(translate::TranslateInfoBarDelegate* delegate,
                              PlaceholderRequestCancelHandler* cancel_handler)
    : cancel_handler_(cancel_handler), scoped_observer_(this) {
  scoped_observer_.Add(delegate);
}

PlaceholderRequestCancelHandler::TranslateDelegateObserver::
    ~TranslateDelegateObserver() = default;

void PlaceholderRequestCancelHandler::TranslateDelegateObserver::
    OnTranslateStepChanged(translate::TranslateStep step,
                           translate::TranslateErrors::Type error_type) {
  cancel_handler_->TranslateStepChanged(step);
}

bool PlaceholderRequestCancelHandler::TranslateDelegateObserver::
    IsDeclinedByUser() {
  return false;
}

void PlaceholderRequestCancelHandler::TranslateDelegateObserver::
    OnTranslateInfoBarDelegateDestroyed(
        translate::TranslateInfoBarDelegate* delegate) {
  scoped_observer_.Remove(delegate);
}

}  // namespace translate_infobar_overlays
