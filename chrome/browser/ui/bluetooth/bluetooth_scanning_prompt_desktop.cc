// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/bluetooth/bluetooth_scanning_prompt_desktop.h"

#include "base/check.h"
#include "chrome/browser/ui/bluetooth/bluetooth_scanning_prompt_controller.h"

BluetoothScanningPromptDesktop::BluetoothScanningPromptDesktop(
    BluetoothScanningPromptController* bluetooth_scanning_prompt_controller,
    base::OnceClosure&& close_closure)
    : bluetooth_scanning_prompt_controller_(
          bluetooth_scanning_prompt_controller),
      close_closure_(std::move(close_closure)) {
  DCHECK(bluetooth_scanning_prompt_controller_);
}

BluetoothScanningPromptDesktop::~BluetoothScanningPromptDesktop() {
  // This satisfies the WebContentsDelegate::ShowBluetoothScanningPrompt()
  // requirement that the EventHandler can be destroyed any time after the
  // BluetoothScanningPrompt instance.
  bluetooth_scanning_prompt_controller_->ResetEventHandler();
  if (close_closure_)
    std::move(close_closure_).Run();
}

void BluetoothScanningPromptDesktop::AddOrUpdateDevice(
    const std::string& device_id,
    bool should_update_name,
    const base::string16& device_name) {
  bluetooth_scanning_prompt_controller_->AddOrUpdateDevice(
      device_id, should_update_name, device_name);
}
