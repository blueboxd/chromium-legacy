// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ios/chrome/browser/overlays/overlay_callback_manager_impl.h"

#include "base/logging.h"

OverlayCallbackManagerImpl::OverlayCallbackManagerImpl() = default;

OverlayCallbackManagerImpl::~OverlayCallbackManagerImpl() {
  // The completion callbacks must be executed before destruction.
  DCHECK(completion_callbacks_.empty());
}

void OverlayCallbackManagerImpl::ExecuteCompletionCallbacks() {
  for (auto& callback : completion_callbacks_) {
    DCHECK(!callback.is_null());
    std::move(callback).Run(GetCompletionResponse());
  }
  completion_callbacks_.clear();
}

void OverlayCallbackManagerImpl::SetCompletionResponse(
    std::unique_ptr<OverlayResponse> response) {
  completion_response_ = std::move(response);
}

OverlayResponse* OverlayCallbackManagerImpl::GetCompletionResponse() const {
  return completion_response_.get();
}

void OverlayCallbackManagerImpl::AddCompletionCallback(
    OverlayCompletionCallback callback) {
  DCHECK(!callback.is_null());
  completion_callbacks_.emplace_back(std::move(callback));
}

void OverlayCallbackManagerImpl::DispatchResponse(
    std::unique_ptr<OverlayResponse> response) {
  DCHECK(response);
  dispatch_callback_storage_.DispatchResponse(response.get());
}

OverlayDispatchCallbackStorage*
OverlayCallbackManagerImpl::GetDispatchCallbackStorage() {
  return &dispatch_callback_storage_;
}
