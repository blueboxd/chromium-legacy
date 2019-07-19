// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/sharing/click_to_call/click_to_call_context_menu_observer.h"

#include "base/bind.h"
#include "base/strings/utf_string_conversions.h"
#include "build/build_config.h"
#include "chrome/app/chrome_command_ids.h"
#include "chrome/app/vector_icons/vector_icons.h"
#include "chrome/browser/renderer_context_menu/render_view_context_menu.h"
#include "chrome/browser/sharing/click_to_call/click_to_call_constants.h"
#include "chrome/browser/sharing/click_to_call/feature.h"
#include "chrome/browser/sharing/sharing_metrics.h"
#include "chrome/browser/sharing/sharing_service.h"
#include "chrome/browser/sharing/sharing_service_factory.h"
#include "chrome/grit/generated_resources.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/gfx/color_palette.h"
#include "ui/gfx/paint_vector_icon.h"
#include "ui/native_theme/native_theme.h"
#include "ui/views/controls/menu/menu_config.h"
#include "url/url_constants.h"

using SharingMessage = chrome_browser_sharing::SharingMessage;

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
      sharing_service_(SharingServiceFactory::GetForBrowserContext(
          proxy_->GetBrowserContext())) {}

ClickToCallContextMenuObserver::~ClickToCallContextMenuObserver() = default;

void ClickToCallContextMenuObserver::InitMenu(
    const content::ContextMenuParams& params) {
  url_ = params.link_url;
  devices_ = sharing_service_->GetDeviceCandidates(
      static_cast<int>(SharingDeviceCapability::kTelephony));
  LogClickToCallDevicesToShow(devices_.size());
  if (devices_.empty())
    return;

  proxy_->AddSeparator();
  if (devices_.size() == 1) {
#if defined(OS_MACOSX)
    proxy_->AddMenuItem(
        IDC_CONTENT_CONTEXT_SHARING_CLICK_TO_CALL_SINGLE_DEVICE,
        l10n_util::GetStringFUTF16(
            IDS_CONTENT_CONTEXT_SHARING_CLICK_TO_CALL_SINGLE_DEVICE,
            base::UTF8ToUTF16(devices_[0].human_readable_name())));
#else
    proxy_->AddMenuItemWithIcon(
        IDC_CONTENT_CONTEXT_SHARING_CLICK_TO_CALL_SINGLE_DEVICE,
        l10n_util::GetStringFUTF16(
            IDS_CONTENT_CONTEXT_SHARING_CLICK_TO_CALL_SINGLE_DEVICE,
            base::UTF8ToUTF16(devices_[0].human_readable_name())),
        GetContextMenuIcon());
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
        sub_menu_model_.get(), GetContextMenuIcon());
#endif
  }
}

void ClickToCallContextMenuObserver::BuildSubMenu() {
  sub_menu_model_ = std::make_unique<ui::SimpleMenuModel>(&sub_menu_delegate_);

  int command_id = kSubMenuFirstDeviceCommandId;
  for (const auto& device : devices_) {
    if (command_id > kSubMenuLastDeviceCommandId)
      break;
    sub_menu_model_->AddItem(command_id++,
                             base::UTF8ToUTF16(device.human_readable_name()));
  }
}

bool ClickToCallContextMenuObserver::IsCommandIdSupported(int command_id) {
  if (devices_.empty())
    return false;

  if (devices_.size() == 1) {
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
    DCHECK(devices_.size() == 1);
    SendClickToCallMessage(0);
  }
}

void ClickToCallContextMenuObserver::SendClickToCallMessage(
    int chosen_device_index) {
  if (chosen_device_index >= static_cast<int>(devices_.size()))
    return;

  LogClickToCallSelectedDeviceIndex(kSharingClickToCallUiContextMenu,
                                    chosen_device_index);

  SharingMessage sharing_message;
  sharing_message.mutable_click_to_call_message()->set_phone_number(
      url_.GetContent());

  sharing_service_->SendMessageToDevice(
      devices_[chosen_device_index].guid(), kSharingClickToCallMessageTTL,
      sharing_message,
      base::BindOnce(&ClickToCallContextMenuObserver::OnMessageSent,
                     AsWeakPtr()));
}

void ClickToCallContextMenuObserver::OnMessageSent(bool sucess) const {
  // TODO(himanshujaju) Add metrics.
}

gfx::ImageSkia ClickToCallContextMenuObserver::GetContextMenuIcon() const {
  bool dark_mode_enabled =
      ui::NativeTheme::GetInstanceForNativeUi()->SystemDarkModeEnabled();
  const gfx::VectorIcon* vector_icon = &kHardwareSmartphoneIcon;
  const views::MenuConfig& menu_config = views::MenuConfig::instance();
  return gfx::CreateVectorIcon(
      *vector_icon, menu_config.touchable_icon_size,
      dark_mode_enabled ? gfx::kGoogleGrey500 : gfx::kChromeIconGrey);
}
