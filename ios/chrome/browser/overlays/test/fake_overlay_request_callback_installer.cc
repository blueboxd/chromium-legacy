// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ios/chrome/browser/overlays/test/fake_overlay_request_callback_installer.h"

#include "base/bind.h"
#include "base/callback.h"
#include "base/logging.h"
#include "ios/chrome/browser/overlays/public/overlay_callback_manager.h"
#include "ios/chrome/browser/overlays/public/overlay_dispatch_callback.h"
#include "ios/chrome/browser/overlays/public/overlay_request_support.h"
#include "ios/chrome/browser/overlays/public/overlay_response_support.h"

#pragma mark - MockOverlayRequestCallbackReceiver

MockOverlayRequestCallbackReceiver::MockOverlayRequestCallbackReceiver() =
    default;

MockOverlayRequestCallbackReceiver::~MockOverlayRequestCallbackReceiver() =
    default;

#pragma mark - FakeOverlayRequestCallbackInstaller

FakeOverlayRequestCallbackInstaller::FakeOverlayRequestCallbackInstaller(
    FakeOverlayRequestCallbackReceiver* receiver)
    : receiver_(receiver), request_support_(OverlayRequestSupport::All()) {
  DCHECK(receiver_);
}

FakeOverlayRequestCallbackInstaller::~FakeOverlayRequestCallbackInstaller() =
    default;

#pragma mark - Public

void FakeOverlayRequestCallbackInstaller::SetRequestSupport(
    const OverlayRequestSupport* request_support) {
  DCHECK(request_support);
  request_support_ = request_support;
}

void FakeOverlayRequestCallbackInstaller::
    StartInstallingDispatchCallbacksWithSupport(
        const OverlayResponseSupport* response_support) {
  DCHECK(response_support);
  dispatch_response_supports_.insert(response_support);
}

#pragma mark - OverlayRequestCallbackInstaller

const OverlayRequestSupport*
FakeOverlayRequestCallbackInstaller::GetRequestSupport() const {
  return request_support_;
}

void FakeOverlayRequestCallbackInstaller::InstallCallbacksInternal(
    OverlayRequest* request) {
  OverlayCallbackManager* manager = request->GetCallbackManager();
  manager->AddCompletionCallback(
      base::BindOnce(&FakeOverlayRequestCallbackReceiver::CompletionCallback,
                     base::Unretained(receiver_), request));
  for (const OverlayResponseSupport* support : dispatch_response_supports_) {
    manager->AddDispatchCallback(OverlayDispatchCallback(
        base::BindRepeating(
            &FakeOverlayRequestCallbackReceiver::DispatchCallback,
            base::Unretained(receiver_), request, support),
        support));
  }
}
