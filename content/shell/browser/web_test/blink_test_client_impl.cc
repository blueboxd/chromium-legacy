// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/shell/browser/web_test/blink_test_client_impl.h"

#include <memory>
#include <string>
#include <utility>

#include "content/shell/browser/web_test/blink_test_controller.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/self_owned_receiver.h"

namespace content {

// static
void BlinkTestClientImpl::Create(
    mojo::PendingReceiver<mojom::BlinkTestClient> receiver) {
  mojo::MakeSelfOwnedReceiver(std::make_unique<BlinkTestClientImpl>(),
                              std::move(receiver));
}

void BlinkTestClientImpl::InitiateLayoutDump() {
  BlinkTestController::Get()->OnInitiateLayoutDump();
}

void BlinkTestClientImpl::ResetDone() {
  BlinkTestController::Get()->OnResetDone();
}

void BlinkTestClientImpl::PrintMessageToStderr(const std::string& message) {
  BlinkTestController::Get()->OnPrintMessageToStderr(message);
}

void BlinkTestClientImpl::Reload() {
  BlinkTestController::Get()->OnReload();
}

void BlinkTestClientImpl::OverridePreferences(
    const content::WebPreferences& web_preferences) {
  BlinkTestController::Get()->OnOverridePreferences(web_preferences);
}

void BlinkTestClientImpl::CloseRemainingWindows() {
  BlinkTestController::Get()->OnCloseRemainingWindows();
}

void BlinkTestClientImpl::GoToOffset(int offset) {
  BlinkTestController::Get()->OnGoToOffset(offset);
}

void BlinkTestClientImpl::SendBluetoothManualChooserEvent(
    const std::string& event,
    const std::string& argument) {
  BlinkTestController::Get()->OnSendBluetoothManualChooserEvent(event,
                                                                argument);
}

void BlinkTestClientImpl::SetBluetoothManualChooser(bool enable) {
  BlinkTestController::Get()->OnSetBluetoothManualChooser(enable);
}

void BlinkTestClientImpl::GetBluetoothManualChooserEvents() {
  BlinkTestController::Get()->OnGetBluetoothManualChooserEvents();
}

void BlinkTestClientImpl::SetPopupBlockingEnabled(bool block_popups) {
  BlinkTestController::Get()->OnSetPopupBlockingEnabled(block_popups);
}

void BlinkTestClientImpl::LoadURLForFrame(const GURL& url,
                                          const std::string& frame_name) {
  BlinkTestController::Get()->OnLoadURLForFrame(url, frame_name);
}

void BlinkTestClientImpl::NavigateSecondaryWindow(const GURL& url) {
  BlinkTestController::Get()->OnNavigateSecondaryWindow(url);
}

}  // namespace content
