// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/infobars/infobar_overlay_request_cancel_handler.h"

#include "base/logging.h"
#include "components/infobars/core/infobar.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

using infobars::InfoBar;
using infobars::InfoBarManager;

#pragma mark - InfobarOverlayRequestCancelHandler

InfobarOverlayRequestCancelHandler::InfobarOverlayRequestCancelHandler(
    OverlayRequest* request,
    OverlayRequestQueue* queue,
    InfoBar* infobar)
    : OverlayRequestCancelHandler(request, queue),
      removal_observer_(infobar, this) {}

InfobarOverlayRequestCancelHandler::~InfobarOverlayRequestCancelHandler() =
    default;

void InfobarOverlayRequestCancelHandler::Cancel() {
  CancelRequest();
}

#pragma mark - InfobarOverlayRequestCancelHandler::RemovalObserver

InfobarOverlayRequestCancelHandler::RemovalObserver::RemovalObserver(
    InfoBar* infobar,
    InfobarOverlayRequestCancelHandler* cancel_handler)
    : infobar_(infobar),
      cancel_handler_(cancel_handler),
      scoped_observer_(this) {
  DCHECK(infobar_);
  DCHECK(cancel_handler_);
  InfoBarManager* manager = infobar_->owner();
  DCHECK(manager);
  scoped_observer_.Add(manager);
}

InfobarOverlayRequestCancelHandler::RemovalObserver::~RemovalObserver() =
    default;

void InfobarOverlayRequestCancelHandler::RemovalObserver::OnInfoBarRemoved(
    infobars::InfoBar* infobar,
    bool animate) {
  if (infobar_ == infobar)
    cancel_handler_->Cancel();
}

void InfobarOverlayRequestCancelHandler::RemovalObserver::OnManagerShuttingDown(
    infobars::InfoBarManager* manager) {
  cancel_handler_->Cancel();
  scoped_observer_.Remove(manager);
}
