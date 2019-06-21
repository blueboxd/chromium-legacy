// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/accessibility/switch_access_event_handler_delegate.h"

#include <utility>

#include "ash/public/interfaces/constants.mojom.h"
#include "chrome/browser/chromeos/accessibility/accessibility_manager.h"
#include "chrome/browser/chromeos/accessibility/event_handler_common.h"
#include "chrome/browser/chromeos/profiles/profile_helper.h"
#include "chrome/common/extensions/api/accessibility_private.h"
#include "chrome/common/extensions/extension_constants.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/system_connector.h"
#include "extensions/browser/event_router.h"
#include "extensions/browser/extension_host.h"
#include "services/service_manager/public/cpp/connector.h"
#include "ui/events/event.h"

namespace {

std::string AccessibilityPrivateEnumForCommand(
    ash::mojom::SwitchAccessCommand command) {
  switch (command) {
    case ash::mojom::SwitchAccessCommand::kSelect:
      return extensions::api::accessibility_private::ToString(
          extensions::api::accessibility_private::SWITCH_ACCESS_COMMAND_SELECT);
    case ash::mojom::SwitchAccessCommand::kNext:
      return extensions::api::accessibility_private::ToString(
          extensions::api::accessibility_private::SWITCH_ACCESS_COMMAND_NEXT);
    case ash::mojom::SwitchAccessCommand::kPrevious:
      return extensions::api::accessibility_private::ToString(
          extensions::api::accessibility_private::
              SWITCH_ACCESS_COMMAND_PREVIOUS);
    case ash::mojom::SwitchAccessCommand::kNone:
      NOTREACHED();
      return "";
  }
}

}  // namespace

SwitchAccessEventHandlerDelegate::SwitchAccessEventHandlerDelegate()
    : binding_(this) {
  // Connect to ash's AccessibilityController interface.
  ash::mojom::AccessibilityControllerPtr accessibility_controller;
  content::GetSystemConnector()->BindInterface(ash::mojom::kServiceName,
                                               &accessibility_controller);

  // Set this object as the SwitchAccessEventHandlerDelegate.
  ash::mojom::SwitchAccessEventHandlerDelegatePtr ptr;
  binding_.Bind(mojo::MakeRequest(&ptr));
  accessibility_controller->SetSwitchAccessEventHandlerDelegate(std::move(ptr));
}

SwitchAccessEventHandlerDelegate::~SwitchAccessEventHandlerDelegate() = default;

void SwitchAccessEventHandlerDelegate::DispatchKeyEvent(
    std::unique_ptr<ui::Event> event) {
  // We can only call the Switch Access extension on the UI thread, make sure we
  // don't ever try to run this code on some other thread.
  DCHECK(content::BrowserThread::CurrentlyOn(content::BrowserThread::UI));
  DCHECK(event->IsKeyEvent());

  extensions::ExtensionHost* host = chromeos::GetAccessibilityExtensionHost(
      extension_misc::kSwitchAccessExtensionId);
  if (!host)
    return;

  const ui::KeyEvent* key_event = event->AsKeyEvent();
  chromeos::ForwardKeyToExtension(*key_event, host);
}

void SwitchAccessEventHandlerDelegate::SendSwitchAccessCommand(
    ash::mojom::SwitchAccessCommand command) {
  extensions::EventRouter* event_router = extensions::EventRouter::Get(
      chromeos::AccessibilityManager::Get()->profile());

  auto event_args = std::make_unique<base::ListValue>();
  event_args->AppendString(AccessibilityPrivateEnumForCommand(command));

  auto event = std::make_unique<extensions::Event>(
      extensions::events::ACCESSIBILITY_PRIVATE_ON_SWITCH_ACCESS_COMMAND,
      extensions::api::accessibility_private::OnSwitchAccessCommand::kEventName,
      std::move(event_args));

  event_router->DispatchEventWithLazyListener(
      extension_misc::kSwitchAccessExtensionId, std::move(event));
}
