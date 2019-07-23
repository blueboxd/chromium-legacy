// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/overlays/overlay_request_queue_impl.h"

#include <utility>

#include "base/logging.h"
#include "base/memory/ptr_util.h"
#include "ios/chrome/browser/overlays/public/overlay_request.h"
#import "ios/web/public/navigation/navigation_context.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

#pragma mark - Factory method

OverlayRequestQueue* OverlayRequestQueue::FromWebState(
    web::WebState* web_state,
    OverlayModality modality) {
  OverlayRequestQueueImpl::Container::CreateForWebState(web_state);
  return OverlayRequestQueueImpl::Container::FromWebState(web_state)
      ->QueueForModality(modality);
}

#pragma mark - OverlayRequestQueueImpl::Container

WEB_STATE_USER_DATA_KEY_IMPL(OverlayRequestQueueImpl::Container)

OverlayRequestQueueImpl::Container::Container(web::WebState* web_state)
    : web_state_(web_state) {}
OverlayRequestQueueImpl::Container::~Container() = default;

OverlayRequestQueueImpl* OverlayRequestQueueImpl::Container::QueueForModality(
    OverlayModality modality) {
  auto& queue = queues_[modality];
  if (!queue)
    queue = base::WrapUnique(new OverlayRequestQueueImpl(web_state_));
  return queue.get();
}

#pragma mark - OverlayRequestQueueImpl

OverlayRequestQueueImpl::OverlayRequestQueueImpl(web::WebState* web_state)
    : cancellation_helper_(this, web_state), weak_factory_(this) {}
OverlayRequestQueueImpl::~OverlayRequestQueueImpl() = default;

#pragma mark Public

void OverlayRequestQueueImpl::AddObserver(Observer* observer) {
  observers_.AddObserver(observer);
}

void OverlayRequestQueueImpl::RemoveObserver(Observer* observer) {
  observers_.RemoveObserver(observer);
}

base::WeakPtr<OverlayRequestQueueImpl> OverlayRequestQueueImpl::GetWeakPtr() {
  return weak_factory_.GetWeakPtr();
}

std::unique_ptr<OverlayRequest> OverlayRequestQueueImpl::PopFrontRequest() {
  DCHECK(!requests_.empty());
  std::unique_ptr<OverlayRequest> request = std::move(requests_.front());
  requests_.pop_front();
  return request;
}

std::unique_ptr<OverlayRequest> OverlayRequestQueueImpl::PopBackRequest() {
  DCHECK(!requests_.empty());
  std::unique_ptr<OverlayRequest> request = std::move(requests_.back());
  requests_.pop_back();
  return request;
}

#pragma mark OverlayRequestQueue

void OverlayRequestQueueImpl::AddRequest(
    std::unique_ptr<OverlayRequest> request) {
  requests_.push_back(std::move(request));
  for (auto& observer : observers_) {
    observer.RequestAddedToQueue(this, requests_.back().get());
  }
}

OverlayRequest* OverlayRequestQueueImpl::front_request() const {
  return requests_.empty() ? nullptr : requests_.front().get();
}

void OverlayRequestQueueImpl::CancelAllRequests() {
  while (!empty()) {
    // Requests are cancelled in reverse order to prevent attempting to present
    // subsequent requests after the dismissal of the front request's UI.
    for (auto& observer : observers_) {
      observer.QueuedRequestCancelled(this, requests_.back().get());
    }
    PopBackRequest();
  }
}

#pragma mark RequestCancellationHelper

OverlayRequestQueueImpl::RequestCancellationHelper::RequestCancellationHelper(
    OverlayRequestQueueImpl* queue,
    web::WebState* web_state)
    : queue_(queue) {
  web_state->AddObserver(this);
}

void OverlayRequestQueueImpl::RequestCancellationHelper::DidFinishNavigation(
    web::WebState* web_state,
    web::NavigationContext* navigation_context) {
  if (navigation_context->HasCommitted() &&
      !navigation_context->IsSameDocument()) {
    queue_->CancelAllRequests();
  }
}

void OverlayRequestQueueImpl::RequestCancellationHelper::RenderProcessGone(
    web::WebState* web_state) {
  queue_->CancelAllRequests();
}

void OverlayRequestQueueImpl::RequestCancellationHelper::WebStateDestroyed(
    web::WebState* web_state) {
  queue_->CancelAllRequests();
  web_state->RemoveObserver(this);
}
