// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/sharing/click_to_call/click_to_call_context_menu_observer.h"

#include "base/bind.h"
#include "base/strings/utf_string_conversions.h"
#include "build/build_config.h"
#include "chrome/app/chrome_command_ids.h"
#include "chrome/browser/renderer_context_menu/render_view_context_menu.h"
#include "chrome/browser/sharing/click_to_call/click_to_call_ui_controller.h"
#include "chrome/browser/sharing/click_to_call/feature.h"
#include "chrome/browser/sharing/sharing_constants.h"
#include "chrome/browser/sharing/sharing_metrics.h"
#include "chrome/grit/generated_resources.h"
#include "components/sync_device_info/device_info.h"
#include "components/vector_icons/vector_icons.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/gfx/color_palette.h"
#include "ui/gfx/paint_vector_icon.h"
#include "url/url_constants.h"

ClickToCallContextMenuObserver::SubMenuDelegate::SubMenuDelegate(
    ClickToCallContextMenuObserver* parent)
    : parent_(parent) {}

ClickToCallContextMenuObserver::SubMenuDelegate::~SubMenuDelegate() = default;

bool ClickToCallContextMenuObserver::SubMenuDelegate::IsCommandIdEnabled(
    int command_id) const {
  // All supported commands are enabled in sub menu.
  return true;
}

void ClickToCallContextMenuObserver::SubMenuDelegate::ExecuteCommand(
    int command_id,
    int event_flags) {
  if (command_id < kSubMenuFirstDeviceCommandId ||
      command_id > kSubMenuLastDeviceCommandId)
    return;
  int device_index = command_id - kSubMenuFirstDeviceCommandId;
  parent_->SendClickToCallMessage(device_index);
}

ClickToCallContextMenuObserver::ClickToCallContextMenuObserver(
    RenderViewContextMenuProxy* proxy)
    : proxy_(proxy),
      controller_(ClickToCallUiController::GetOrCreateFromWebContents(
          proxy_->GetWebContents())) {}

ClickToCallContextMenuObserver::~ClickToCallContextMenuObserver() = default;

void ClickToCallContextMenuObserver::InitMenu(
    const content::ContextMenuParams& params) {
  url_ = params.link_url;
  controller_->UpdateDevices();
  const std::vector<std::unique_ptr<syncer::DeviceInfo>>& devices =
      controller_->devices();
  LogClickToCallDevicesToShow(kSharingClickToCallUiContextMenu, devices.size());
  if (devices.empty())
    return;

  proxy_->AddSeparator();
  if (devices.size() == 1) {
#if defined(OS_MACOSX)
    proxy_->AddMenuItem(
        IDC_CONTENT_CONTEXT_SHARING_CLICK_TO_CALL_SINGLE_DEVICE,
        l10n_util::GetStringFUTF16(
            IDS_CONTENT_CONTEXT_SHARING_CLICK_TO_CALL_SINGLE_DEVICE,
            base::UTF8ToUTF16(devices[0]->client_name())));
#else
    proxy_->AddMenuItemWithIcon(
        IDC_CONTENT_CONTEXT_SHARING_CLICK_TO_CALL_SINGLE_DEVICE,
        l10n_util::GetStringFUTF16(
            IDS_CONTENT_CONTEXT_SHARING_CLICK_TO_CALL_SINGLE_DEVICE,
            base::UTF8ToUTF16(devices[0]->client_name())),
        vector_icons::kCallIcon);
#endif
  } else {
    BuildSubMenu();
#if defined(OS_MACOSX)
    proxy_->AddSubMenu(
        IDC_CONTENT_CONTEXT_SHARING_CLICK_TO_CALL_MULTIPLE_DEVICES,
        l10n_util::GetStringUTF16(
            IDS_CONTENT_CONTEXT_SHARING_CLICK_TO_CALL_MULTIPLE_DEVICES),
        sub_menu_model_.get());
#else
    proxy_->AddSubMenuWithStringIdAndIcon(
        IDC_CONTENT_CONTEXT_SHARING_CLICK_TO_CALL_MULTIPLE_DEVICES,
        IDS_CONTENT_CONTEXT_SHARING_CLICK_TO_CALL_MULTIPLE_DEVICES,
        sub_menu_model_.get(), vector_icons::kCallIcon);
#endif
  }
}

void ClickToCallContextMenuObserver::BuildSubMenu() {
  sub_menu_model_ = std::make_unique<ui::SimpleMenuModel>(&sub_menu_delegate_);

  int command_id = kSubMenuFirstDeviceCommandId;
  for (const auto& device : controller_->devices()) {
    if (command_id > kSubMenuLastDeviceCommandId)
      break;
    sub_menu_model_->AddItem(command_id++,
                             base::UTF8ToUTF16(device->client_name()));
  }
}

bool ClickToCallContextMenuObserver::IsCommandIdSupported(int command_id) {
  size_t device_count = controller_->devices().size();
  if (device_count == 0)
    return false;

  if (device_count == 1) {
    return command_id ==
           IDC_CONTENT_CONTEXT_SHARING_CLICK_TO_CALL_SINGLE_DEVICE;
  } else {
    return command_id ==
           IDC_CONTENT_CONTEXT_SHARING_CLICK_TO_CALL_MULTIPLE_DEVICES;
  }
}

bool ClickToCallContextMenuObserver::IsCommandIdEnabled(int command_id) {
  // All supported commands are enabled.
  return true;
}

void ClickToCallContextMenuObserver::ExecuteCommand(int command_id) {
  if (command_id == IDC_CONTENT_CONTEXT_SHARING_CLICK_TO_CALL_SINGLE_DEVICE) {
    DCHECK(controller_->devices().size() == 1);
    SendClickToCallMessage(0);
  }
}

void ClickToCallContextMenuObserver::SendClickToCallMessage(
    int chosen_device_index) {
  const std::vector<std::unique_ptr<syncer::DeviceInfo>>& devices =
      controller_->devices();
  if (chosen_device_index >= static_cast<int>(devices.size()))
    return;

  LogClickToCallSelectedDeviceIndex(kSharingClickToCallUiContextMenu,
                                    chosen_device_index);

  controller_->OnDeviceSelected(url_, *devices[chosen_device_index]);
}
