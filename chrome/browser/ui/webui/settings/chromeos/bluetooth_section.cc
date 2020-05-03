// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/settings/chromeos/bluetooth_section.h"

#include "base/bind.h"
#include "base/no_destructor.h"
#include "chrome/browser/ui/webui/chromeos/bluetooth_dialog_localized_strings_provider.h"
#include "chrome/browser/ui/webui/settings/chromeos/constants/routes.mojom.h"
#include "chrome/browser/ui/webui/settings/chromeos/constants/setting.mojom.h"
#include "chrome/browser/ui/webui/settings/chromeos/search/search.mojom.h"
#include "chrome/browser/ui/webui/settings/chromeos/search/search_result_icon.mojom.h"
#include "chrome/browser/ui/webui/webui_util.h"
#include "chrome/common/webui_url_constants.h"
#include "chrome/grit/generated_resources.h"
#include "content/public/browser/web_ui_data_source.h"
#include "device/bluetooth/bluetooth_adapter_factory.h"
#include "device/bluetooth/dbus/bluez_dbus_manager.h"
#include "device/bluetooth/strings/grit/bluetooth_strings.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/webui/web_ui_util.h"

namespace chromeos {
namespace settings {
namespace {

const std::vector<SearchConcept>& GetBluetoothSearchConcepts() {
  static const base::NoDestructor<std::vector<SearchConcept>> tags({
      {IDS_OS_SETTINGS_TAG_BLUETOOTH_PAIR,
       mojom::kBluetoothDevicesSubpagePath,
       mojom::SearchResultIcon::kBluetooth,
       mojom::SearchResultDefaultRank::kHigh,
       mojom::SearchResultType::kSetting,
       {.setting = mojom::Setting::kBluetoothPairDevice}},
      {IDS_OS_SETTINGS_TAG_BLUETOOTH_CONNECT,
       mojom::kBluetoothDevicesSubpagePath,
       mojom::SearchResultIcon::kBluetooth,
       mojom::SearchResultDefaultRank::kHigh,
       mojom::SearchResultType::kSetting,
       {.setting = mojom::Setting::kBluetoothConnectToDevice}},
      {IDS_OS_SETTINGS_TAG_BLUETOOTH,
       mojom::kBluetoothDevicesSubpagePath,
       mojom::SearchResultIcon::kBluetooth,
       mojom::SearchResultDefaultRank::kHigh,
       mojom::SearchResultType::kSubpage,
       {.subpage = mojom::Subpage::kBluetoothDevices}},
      {IDS_OS_SETTINGS_TAG_BLUETOOTH_DISCONNECT,
       mojom::kBluetoothDevicesSubpagePath,
       mojom::SearchResultIcon::kBluetooth,
       mojom::SearchResultDefaultRank::kHigh,
       mojom::SearchResultType::kSetting,
       {.setting = mojom::Setting::kBluetoothDisconnectFromDevice}},
      {IDS_OS_SETTINGS_TAG_BLUETOOTH_UNPAIR,
       mojom::kBluetoothDevicesSubpagePath,
       mojom::SearchResultIcon::kBluetooth,
       mojom::SearchResultDefaultRank::kHigh,
       mojom::SearchResultType::kSetting,
       {.setting = mojom::Setting::kBluetoothUnpairDevice},
       {IDS_OS_SETTINGS_TAG_BLUETOOTH_UNPAIR_ALT1, SearchConcept::kAltTagEnd}},
  });
  return *tags;
}

const std::vector<SearchConcept>& GetBluetoothOnSearchConcepts() {
  static const base::NoDestructor<std::vector<SearchConcept>> tags({
      {IDS_OS_SETTINGS_TAG_BLUETOOTH_TURN_OFF,
       mojom::kBluetoothDevicesSubpagePath,
       mojom::SearchResultIcon::kBluetooth,
       mojom::SearchResultDefaultRank::kHigh,
       mojom::SearchResultType::kSetting,
       {.setting = mojom::Setting::kBluetoothOnOff},
       {IDS_OS_SETTINGS_TAG_BLUETOOTH_TURN_OFF_ALT1,
        SearchConcept::kAltTagEnd}},
  });
  return *tags;
}

const std::vector<SearchConcept>& GetBluetoothOffSearchConcepts() {
  static const base::NoDestructor<std::vector<SearchConcept>> tags({
      {IDS_OS_SETTINGS_TAG_BLUETOOTH_TURN_ON,
       mojom::kBluetoothDevicesSubpagePath,
       mojom::SearchResultIcon::kBluetooth,
       mojom::SearchResultDefaultRank::kHigh,
       mojom::SearchResultType::kSetting,
       {.setting = mojom::Setting::kBluetoothOnOff},
       {IDS_OS_SETTINGS_TAG_BLUETOOTH_TURN_ON_ALT1, SearchConcept::kAltTagEnd}},
  });
  return *tags;
}

}  // namespace

BluetoothSection::BluetoothSection(Profile* profile,
                                   Delegate* per_page_delegate)
    : OsSettingsSection(profile, per_page_delegate) {
  // Note: May be uninitialized in tests.
  if (bluez::BluezDBusManager::IsInitialized()) {
    device::BluetoothAdapterFactory::Get()->GetAdapter(
        base::Bind(&BluetoothSection::OnFetchBluetoothAdapter,
                   weak_ptr_factory_.GetWeakPtr()));
  }
}

BluetoothSection::~BluetoothSection() {
  if (bluetooth_adapter_)
    bluetooth_adapter_->RemoveObserver(this);
}

void BluetoothSection::AddLoadTimeData(content::WebUIDataSource* html_source) {
  static constexpr webui::LocalizedString kLocalizedStrings[] = {
      {"bluetoothConnected", IDS_SETTINGS_BLUETOOTH_CONNECTED},
      {"bluetoothConnectedWithBattery",
       IDS_SETTINGS_BLUETOOTH_CONNECTED_WITH_BATTERY},
      {"bluetoothConnecting", IDS_SETTINGS_BLUETOOTH_CONNECTING},
      {"bluetoothDeviceListPaired", IDS_SETTINGS_BLUETOOTH_DEVICE_LIST_PAIRED},
      {"bluetoothDeviceListUnpaired",
       IDS_SETTINGS_BLUETOOTH_DEVICE_LIST_UNPAIRED},
      {"bluetoothConnect", IDS_SETTINGS_BLUETOOTH_CONNECT},
      {"bluetoothDisconnect", IDS_SETTINGS_BLUETOOTH_DISCONNECT},
      {"bluetoothToggleA11yLabel",
       IDS_SETTINGS_BLUETOOTH_TOGGLE_ACCESSIBILITY_LABEL},
      {"bluetoothExpandA11yLabel",
       IDS_SETTINGS_BLUETOOTH_EXPAND_ACCESSIBILITY_LABEL},
      {"bluetoothNoDevices", IDS_SETTINGS_BLUETOOTH_NO_DEVICES},
      {"bluetoothNoDevicesFound", IDS_SETTINGS_BLUETOOTH_NO_DEVICES_FOUND},
      {"bluetoothNotConnected", IDS_SETTINGS_BLUETOOTH_NOT_CONNECTED},
      {"bluetoothPageTitle", IDS_SETTINGS_BLUETOOTH},
      {"bluetoothPairDevicePageTitle",
       IDS_SETTINGS_BLUETOOTH_PAIR_DEVICE_TITLE},
      {"bluetoothRemove", IDS_SETTINGS_BLUETOOTH_REMOVE},
      {"bluetoothPrimaryUserControlled",
       IDS_SETTINGS_BLUETOOTH_PRIMARY_USER_CONTROLLED},
      {"bluetoothDeviceType_computer",
       IDS_BLUETOOTH_ACCESSIBILITY_DEVICE_TYPE_COMPUTER},
      {"bluetoothDeviceType_phone",
       IDS_BLUETOOTH_ACCESSIBILITY_DEVICE_TYPE_PHONE},
      {"bluetoothDeviceType_modem",
       IDS_BLUETOOTH_ACCESSIBILITY_DEVICE_TYPE_MODEM},
      {"bluetoothDeviceType_audio",
       IDS_BLUETOOTH_ACCESSIBILITY_DEVICE_TYPE_AUDIO},
      {"bluetoothDeviceType_carAudio",
       IDS_BLUETOOTH_ACCESSIBILITY_DEVICE_TYPE_CAR_AUDIO},
      {"bluetoothDeviceType_video",
       IDS_BLUETOOTH_ACCESSIBILITY_DEVICE_TYPE_VIDEO},
      {"bluetoothDeviceType_peripheral",
       IDS_BLUETOOTH_ACCESSIBILITY_DEVICE_TYPE_PERIPHERAL},
      {"bluetoothDeviceType_joystick",
       IDS_BLUETOOTH_ACCESSIBILITY_DEVICE_TYPE_JOYSTICK},
      {"bluetoothDeviceType_gamepad",
       IDS_BLUETOOTH_ACCESSIBILITY_DEVICE_TYPE_GAMEPAD},
      {"bluetoothDeviceType_keyboard",
       IDS_BLUETOOTH_ACCESSIBILITY_DEVICE_TYPE_KEYBOARD},
      {"bluetoothDeviceType_mouse",
       IDS_BLUETOOTH_ACCESSIBILITY_DEVICE_TYPE_MOUSE},
      {"bluetoothDeviceType_tablet",
       IDS_BLUETOOTH_ACCESSIBILITY_DEVICE_TYPE_TABLET},
      {"bluetoothDeviceType_keyboardMouseCombo",
       IDS_BLUETOOTH_ACCESSIBILITY_DEVICE_TYPE_KEYBOARD_MOUSE_COMBO},
      {"bluetoothDeviceType_unknown",
       IDS_BLUETOOTH_ACCESSIBILITY_DEVICE_TYPE_UNKNOWN},
  };
  AddLocalizedStringsBulk(html_source, kLocalizedStrings);
  chromeos::bluetooth_dialog::AddLocalizedStrings(html_source);
}

void BluetoothSection::AdapterPresentChanged(device::BluetoothAdapter* adapter,
                                             bool present) {
  UpdateSearchTags();
}

void BluetoothSection::AdapterPoweredChanged(device::BluetoothAdapter* adapter,
                                             bool powered) {
  UpdateSearchTags();
}

void BluetoothSection::OnFetchBluetoothAdapter(
    scoped_refptr<device::BluetoothAdapter> bluetooth_adapter) {
  bluetooth_adapter_ = bluetooth_adapter;
  bluetooth_adapter_->AddObserver(this);
  UpdateSearchTags();
}

void BluetoothSection::UpdateSearchTags() {
  // Start with no search tags, then add them below if appropriate.
  delegate()->RemoveSearchTags(GetBluetoothSearchConcepts());
  delegate()->RemoveSearchTags(GetBluetoothOnSearchConcepts());
  delegate()->RemoveSearchTags(GetBluetoothOffSearchConcepts());

  if (!bluetooth_adapter_->IsPresent())
    return;

  delegate()->AddSearchTags(GetBluetoothSearchConcepts());

  if (bluetooth_adapter_->IsPowered())
    delegate()->AddSearchTags(GetBluetoothOnSearchConcepts());
  else
    delegate()->AddSearchTags(GetBluetoothOffSearchConcepts());
}

}  // namespace settings
}  // namespace chromeos
