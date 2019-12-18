// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ios/chrome/browser/overlays/public/overlay_dispatch_callback_storage.h"

#pragma mark - OverlayDispatchCallbackStorage

OverlayDispatchCallbackStorage::OverlayDispatchCallbackStorage() = default;

OverlayDispatchCallbackStorage::~OverlayDispatchCallbackStorage() = default;

void OverlayDispatchCallbackStorage::DispatchResponse(
    OverlayResponse* response) {
  for (auto& list_pair : callback_lists_) {
    list_pair.second->ExecuteCallbacks(response);
  }
}

#pragma mark - OverlayDispatchCallbackStorage::CallbackList

OverlayDispatchCallbackStorage::CallbackList::CallbackList() = default;

OverlayDispatchCallbackStorage::CallbackList::~CallbackList() = default;

void OverlayDispatchCallbackStorage::CallbackList::AddCallback(
    OverlayDispatchCallback callback) {
  DCHECK(!callback.is_null());
  callbacks_.emplace_back(std::move(callback));
}

void OverlayDispatchCallbackStorage::CallbackList::ExecuteCallbacks(
    OverlayResponse* response) {
  if (!ShouldExecuteForResponse(response))
    return;
  for (auto& callback : callbacks_) {
    callback.Run(response);
  }
}
