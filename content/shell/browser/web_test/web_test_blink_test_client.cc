// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/shell/browser/web_test/web_test_blink_test_client.h"

#include <memory>
#include <string>
#include <utility>

#include "content/shell/browser/web_test/blink_test_controller.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/self_owned_receiver.h"

namespace content {

// static
void WebTestBlinkTestClient::Create(
    mojo::PendingReceiver<mojom::WebTestClient> receiver) {
  mojo::MakeSelfOwnedReceiver(std::make_unique<WebTestBlinkTestClient>(),
                              std::move(receiver));
}

void WebTestBlinkTestClient::InitiateLayoutDump() {
  BlinkTestController::Get()->OnInitiateLayoutDump();
}

void WebTestBlinkTestClient::PrintMessageToStderr(const std::string& message) {
  BlinkTestController::Get()->OnPrintMessageToStderr(message);
}

void WebTestBlinkTestClient::Reload() {
  BlinkTestController::Get()->OnReload();
}

void WebTestBlinkTestClient::CloseRemainingWindows() {
  BlinkTestController::Get()->OnCloseRemainingWindows();
}

void WebTestBlinkTestClient::GoToOffset(int offset) {
  BlinkTestController::Get()->OnGoToOffset(offset);
}

void WebTestBlinkTestClient::SendBluetoothManualChooserEvent(
    const std::string& event,
    const std::string& argument) {
  BlinkTestController::Get()->OnSendBluetoothManualChooserEvent(event,
                                                                argument);
}

void WebTestBlinkTestClient::SetBluetoothManualChooser(bool enable) {
  BlinkTestController::Get()->OnSetBluetoothManualChooser(enable);
}

void WebTestBlinkTestClient::GetBluetoothManualChooserEvents() {
  BlinkTestController::Get()->OnGetBluetoothManualChooserEvents();
}

void WebTestBlinkTestClient::SetPopupBlockingEnabled(bool block_popups) {
  BlinkTestController::Get()->OnSetPopupBlockingEnabled(block_popups);
}

void WebTestBlinkTestClient::LoadURLForFrame(const GURL& url,
                                             const std::string& frame_name) {
  BlinkTestController::Get()->OnLoadURLForFrame(url, frame_name);
}

void WebTestBlinkTestClient::NavigateSecondaryWindow(const GURL& url) {
  BlinkTestController::Get()->OnNavigateSecondaryWindow(url);
}

}  // namespace content
