// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/ash/accessibility/fake_accessibility_controller.h"

#include <utility>

#include "ash/public/interfaces/constants.mojom.h"
#include "base/bind.h"
#include "content/public/browser/system_connector.h"
#include "services/service_manager/public/cpp/connector.h"
#include "services/service_manager/public/cpp/service_filter.h"

FakeAccessibilityController::FakeAccessibilityController() {
  CHECK(content::GetSystemConnector())
      << "ServiceManager is uninitialized. Did you forget to create a "
         "content::TestServiceManagerContext?";
  content::GetSystemConnector()->OverrideBinderForTesting(
      service_manager::ServiceFilter::ByName(ash::mojom::kServiceName),
      ash::mojom::AccessibilityController::Name_,
      base::BindRepeating(&FakeAccessibilityController::Bind,
                          base::Unretained(this)));
}

FakeAccessibilityController::~FakeAccessibilityController() {
  content::GetSystemConnector()->ClearBinderOverrideForTesting(
      service_manager::ServiceFilter::ByName(ash::mojom::kServiceName),
      ash::mojom::AccessibilityController::Name_);
}

void FakeAccessibilityController::SetClient(
    ash::mojom::AccessibilityControllerClientPtr client) {
  was_client_set_ = true;
}
void FakeAccessibilityController::SetDarkenScreen(bool darken) {}

void FakeAccessibilityController::BrailleDisplayStateChanged(bool connected) {}

void FakeAccessibilityController::SetFocusHighlightRect(
    const gfx::Rect& bounds_in_screen) {}

void FakeAccessibilityController::SetCaretBounds(
    const gfx::Rect& bounds_in_screen) {}

void FakeAccessibilityController::SetAccessibilityPanelAlwaysVisible(
    bool always_visible) {}

void FakeAccessibilityController::SetAccessibilityPanelBounds(
    const gfx::Rect& bounds,
    ash::mojom::AccessibilityPanelState state) {}

void FakeAccessibilityController::SetSelectToSpeakState(
    ash::mojom::SelectToSpeakState state) {}

void FakeAccessibilityController::SetSelectToSpeakEventHandlerDelegate(
    ash::mojom::SelectToSpeakEventHandlerDelegatePtr delegate) {}

void FakeAccessibilityController::SetSwitchAccessEventHandlerDelegate(
    ash::mojom::SwitchAccessEventHandlerDelegatePtr delegate) {}

void FakeAccessibilityController::SetSwitchAccessKeysToCapture(
    const std::vector<int>& keys_to_capture) {}

void FakeAccessibilityController::ToggleDictationFromSource(
    ash::mojom::DictationToggleSource source) {}

void FakeAccessibilityController::ForwardKeyEventsToSwitchAccess(
    bool should_forward) {}

void FakeAccessibilityController::GetBatteryDescription(
    GetBatteryDescriptionCallback callback) {}

void FakeAccessibilityController::SetVirtualKeyboardVisible(bool is_visible) {}

void FakeAccessibilityController::Bind(mojo::ScopedMessagePipeHandle handle) {
  binding_.Bind(ash::mojom::AccessibilityControllerRequest(std::move(handle)));
}
