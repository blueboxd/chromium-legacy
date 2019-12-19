// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/infobars/overlays/infobar_overlay_request_cancel_handler.h"

#include "base/logging.h"
#include "components/infobars/core/infobar.h"
#import "ios/chrome/browser/infobars/overlays/infobar_overlay_request_inserter.h"
#import "ios/chrome/browser/overlays/public/common/infobars/infobar_overlay_request_config.h"
#import "ios/chrome/browser/overlays/public/overlay_request.h"
#import "ios/chrome/browser/overlays/public/overlay_request_queue.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

using infobars::InfoBar;
using infobars::InfoBarManager;

#pragma mark - InfobarOverlayRequestCancelHandler

InfobarOverlayRequestCancelHandler::InfobarOverlayRequestCancelHandler(
    OverlayRequest* request,
    OverlayRequestQueue* queue,
    InfobarOverlayType type,
    const InfobarOverlayRequestInserter* inserter)
    : OverlayRequestCancelHandler(request, queue),
      type_(type),
      inserter_(inserter),
      infobar_(request->GetConfig<InfobarOverlayRequestConfig>()->infobar()),
      removal_observer_(this) {
  DCHECK(inserter_);
  DCHECK(infobar_);
}

InfobarOverlayRequestCancelHandler::~InfobarOverlayRequestCancelHandler() =
    default;

void InfobarOverlayRequestCancelHandler::Cancel() {
  CancelRequest();
}

void InfobarOverlayRequestCancelHandler::InsertReplacementRequest(
    InfoBar* replacement) {
  size_t index = 0;
  while (index < queue()->size()) {
    InfobarOverlayRequestConfig* config =
        queue()->GetRequest(index)->GetConfig<InfobarOverlayRequestConfig>();
    if (config->infobar() == infobar())
      break;
    ++index;
  }
  DCHECK_LT(index, queue()->size());
  inserter_->InsertOverlayRequest(replacement, type_, index + 1);
}

#pragma mark - InfobarOverlayRequestCancelHandler::RemovalObserver

InfobarOverlayRequestCancelHandler::RemovalObserver::RemovalObserver(
    InfobarOverlayRequestCancelHandler* cancel_handler)
    : cancel_handler_(cancel_handler), scoped_observer_(this) {
  DCHECK(cancel_handler_);
  InfoBarManager* manager = cancel_handler_->infobar()->owner();
  DCHECK(manager);
  scoped_observer_.Add(manager);
}

InfobarOverlayRequestCancelHandler::RemovalObserver::~RemovalObserver() =
    default;

void InfobarOverlayRequestCancelHandler::RemovalObserver::OnInfoBarRemoved(
    infobars::InfoBar* infobar,
    bool animate) {
  if (cancel_handler_->infobar() == infobar)
    cancel_handler_->Cancel();
}

void InfobarOverlayRequestCancelHandler::RemovalObserver::OnInfoBarReplaced(
    InfoBar* old_infobar,
    InfoBar* new_infobar) {
  if (cancel_handler_->infobar() == old_infobar) {
    cancel_handler_->InsertReplacementRequest(new_infobar);
    cancel_handler_->Cancel();
  }
}

void InfobarOverlayRequestCancelHandler::RemovalObserver::OnManagerShuttingDown(
    infobars::InfoBarManager* manager) {
  cancel_handler_->Cancel();
  scoped_observer_.Remove(manager);
}
