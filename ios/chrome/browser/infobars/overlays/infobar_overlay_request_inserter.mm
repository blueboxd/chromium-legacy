// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/infobars/overlays/infobar_overlay_request_inserter.h"

#include "base/logging.h"
#include "base/memory/ptr_util.h"
#include "ios/chrome/browser/infobars/infobar_ios.h"
#import "ios/chrome/browser/infobars/overlays/infobar_banner_overlay_request_cancel_handler.h"
#import "ios/chrome/browser/infobars/overlays/infobar_modal_completion_notifier.h"
#import "ios/chrome/browser/infobars/overlays/infobar_overlay_request_cancel_handler.h"
#import "ios/chrome/browser/infobars/overlays/infobar_overlay_request_factory.h"
#import "ios/chrome/browser/overlays/public/common/infobars/infobar_overlay_request_config.h"
#include "ios/chrome/browser/overlays/public/overlay_modality.h"
#include "ios/chrome/browser/overlays/public/overlay_request.h"
#import "ios/chrome/browser/overlays/public/overlay_request_queue.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

WEB_STATE_USER_DATA_KEY_IMPL(InfobarOverlayRequestInserter)

// static
void InfobarOverlayRequestInserter::CreateForWebState(
    web::WebState* web_state,
    std::unique_ptr<InfobarOverlayRequestFactory> request_factory) {
  DCHECK(web_state);
  if (!FromWebState(web_state)) {
    web_state->SetUserData(UserDataKey(),
                           base::WrapUnique(new InfobarOverlayRequestInserter(
                               web_state, std::move(request_factory))));
  }
}

InsertParams::InsertParams(InfoBarIOS* infobar) : infobar(infobar) {}

InfobarOverlayRequestInserter::InfobarOverlayRequestInserter(
    web::WebState* web_state,
    std::unique_ptr<InfobarOverlayRequestFactory> factory)
    : web_state_(web_state),
      modal_completion_notifier_(
          std::make_unique<InfobarModalCompletionNotifier>(web_state_)),
      request_factory_(std::move(factory)) {
  DCHECK(web_state_);
  DCHECK(request_factory_);
  // Populate |queues_| with the request queues at the appropriate modalities.
  queues_[InfobarOverlayType::kBanner] = OverlayRequestQueue::FromWebState(
      web_state_, OverlayModality::kInfobarBanner);
  queues_[InfobarOverlayType::kDetailSheet] = OverlayRequestQueue::FromWebState(
      web_state_, OverlayModality::kInfobarModal);
  queues_[InfobarOverlayType::kModal] = OverlayRequestQueue::FromWebState(
      web_state_, OverlayModality::kInfobarModal);
}

InfobarOverlayRequestInserter::~InfobarOverlayRequestInserter() {
  for (auto& observer : observers_) {
    observer.InserterDestroyed(this);
  }
}

void InfobarOverlayRequestInserter::AddObserver(Observer* observer) {
  observers_.AddObserver(observer);
}
void InfobarOverlayRequestInserter::RemoveObserver(Observer* observer) {
  observers_.RemoveObserver(observer);
}

void InfobarOverlayRequestInserter::InsertOverlayRequest(
    const InsertParams& params) {
  // Create the request and its cancel handler.
  std::unique_ptr<OverlayRequest> request =
      request_factory_->CreateInfobarRequest(params.infobar,
                                             params.overlay_type);
  // TODO(crbug.com/1030357): Replace early return with a DCHECK once all
  // infobars have been converted to use OverlayPresenter.
  if (!request)
    return;
  InfoBarIOS* infobar_ios = static_cast<InfoBarIOS*>(params.infobar);
  DCHECK_EQ(infobar_ios,
            request->GetConfig<InfobarOverlayRequestConfig>()->infobar());
  OverlayRequestQueue* queue = queues_.at(params.overlay_type);
  std::unique_ptr<OverlayRequestCancelHandler> cancel_handler;
  switch (params.overlay_type) {
    case InfobarOverlayType::kBanner:
      cancel_handler =
          std::make_unique<InfobarBannerOverlayRequestCancelHandler>(
              request.get(), queue, infobar_ios, this,
              modal_completion_notifier_.get());
      break;
    case InfobarOverlayType::kDetailSheet:
    case InfobarOverlayType::kModal:
      cancel_handler = std::make_unique<InfobarOverlayRequestCancelHandler>(
          request.get(), queue, infobar_ios);
      break;
  }
  for (auto& observer : observers_) {
    observer.InfobarRequestInserted(this, params);
  }
  queue->InsertRequest(params.insertion_index, std::move(request),
                       std::move(cancel_handler));
}
