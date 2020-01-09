// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/overlays/overlay_request_queue_impl.h"

#include <utility>

#include "base/logging.h"
#include "base/memory/ptr_util.h"
#import "ios/chrome/browser/overlays/default_overlay_request_cancel_handler.h"
#include "ios/chrome/browser/overlays/overlay_request_impl.h"
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
    : web_state_(web_state), weak_factory_(this) {}
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
  DCHECK(size());
  std::unique_ptr<OverlayRequest> request =
      std::move(request_storages_.front().request);
  request_storages_.pop_front();
  return request;
}

std::unique_ptr<OverlayRequest> OverlayRequestQueueImpl::PopBackRequest() {
  DCHECK(size());
  std::unique_ptr<OverlayRequest> request =
      std::move(request_storages_.back().request);
  request_storages_.pop_back();
  return request;
}

#pragma mark OverlayRequestQueue

size_t OverlayRequestQueueImpl::size() const {
  return request_storages_.size();
}

OverlayRequest* OverlayRequestQueueImpl::front_request() const {
  return size() ? GetRequest(0) : nullptr;
}

OverlayRequest* OverlayRequestQueueImpl::GetRequest(size_t index) const {
  DCHECK_LT(index, size());
  return request_storages_[index].request.get();
}

void OverlayRequestQueueImpl::AddRequest(
    std::unique_ptr<OverlayRequest> request,
    std::unique_ptr<OverlayRequestCancelHandler> cancel_handler) {
  InsertRequest(size(), std::move(request), std::move(cancel_handler));
}

void OverlayRequestQueueImpl::InsertRequest(
    size_t index,
    std::unique_ptr<OverlayRequest> request,
    std::unique_ptr<OverlayRequestCancelHandler> cancel_handler) {
  DCHECK_LE(index, size());
  DCHECK(request.get());
  // Create the cancel handler if necessary.
  if (!cancel_handler) {
    cancel_handler = std::make_unique<DefaultOverlayRequestCancelHandler>(
        request.get(), this, web_state_);
  }
  static_cast<OverlayRequestImpl*>(request.get())
      ->set_queue_web_state(web_state_);
  request_storages_.emplace(request_storages_.begin() + index,
                            std::move(request), std::move(cancel_handler));
  for (auto& observer : observers_) {
    observer.RequestAddedToQueue(this, request_storages_[index].request.get(),
                                 index);
  }
}

void OverlayRequestQueueImpl::CancelAllRequests() {
  while (size()) {
    // Requests are cancelled in reverse order to prevent attempting to present
    // subsequent requests after the dismissal of the front request's UI.
    for (auto& observer : observers_) {
      observer.QueuedRequestCancelled(this,
                                      request_storages_.back().request.get());
    }
    PopBackRequest();
  }
}

void OverlayRequestQueueImpl::CancelRequest(OverlayRequest* request) {
  // Find the iterator for the storage holding |request|.
  auto storage_iter = request_storages_.begin();
  auto end = request_storages_.end();
  while (storage_iter != end) {
    if ((*storage_iter).request.get() == request)
      break;
    ++storage_iter;
  }
  if (storage_iter == end)
    return;

  // Notify observers of cancellation and remove the storage.
  for (auto& observer : observers_) {
    observer.QueuedRequestCancelled(this, request);
  }
  request_storages_.erase(storage_iter);
}

#pragma mark OverlayRequestStorage

OverlayRequestQueueImpl::OverlayRequestStorage::OverlayRequestStorage(
    std::unique_ptr<OverlayRequest> request,
    std::unique_ptr<OverlayRequestCancelHandler> cancel_handler)
    : request(std::move(request)), cancel_handler(std::move(cancel_handler)) {}

OverlayRequestQueueImpl::OverlayRequestStorage::OverlayRequestStorage(
    OverlayRequestQueueImpl::OverlayRequestStorage&& storage)
    : request(std::move(storage.request)),
      cancel_handler(std::move(storage.cancel_handler)) {}

OverlayRequestQueueImpl::OverlayRequestStorage::~OverlayRequestStorage() {}
