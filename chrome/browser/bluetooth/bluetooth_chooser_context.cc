// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/bluetooth/bluetooth_chooser_context.h"

#include "base/values.h"
#include "chrome/browser/profiles/profile.h"
#include "components/content_settings/core/common/content_settings_types.h"
#include "url/origin.h"

using blink::WebBluetoothDeviceId;
using device::BluetoothUUID;
using device::BluetoothUUIDHash;

BluetoothChooserContext::BluetoothChooserContext(Profile* profile)
    : ChooserContextBase(profile,
                         ContentSettingsType::BLUETOOTH_GUARD,
                         ContentSettingsType::BLUETOOTH_CHOOSER_DATA) {}

BluetoothChooserContext::~BluetoothChooserContext() = default;

const WebBluetoothDeviceId BluetoothChooserContext::GetWebBluetoothDeviceId(
    const url::Origin& requesting_origin,
    const url::Origin& embedding_origin,
    const std::string& device_address) {
  NOTIMPLEMENTED();
  return {};
}

const std::string BluetoothChooserContext::GetDeviceAddress(
    const url::Origin& requesting_origin,
    const url::Origin& embedding_origin,
    const WebBluetoothDeviceId& device_id) {
  NOTIMPLEMENTED();
  return "";
}

const WebBluetoothDeviceId BluetoothChooserContext::GrantDevicePermission(
    const url::Origin& requesting_origin,
    const url::Origin& embedding_origin,
    const std::string& device_address,
    base::flat_set<BluetoothUUID, BluetoothUUIDHash>& services) {
  NOTIMPLEMENTED();
  return {};
}

bool BluetoothChooserContext::HasDevicePermission(
    const url::Origin& requesting_origin,
    const url::Origin& embedding_origin,
    const WebBluetoothDeviceId& device_id) {
  NOTIMPLEMENTED();
  return false;
}

bool BluetoothChooserContext::IsAllowedToAccessAtLeastOneService(
    const url::Origin& requesting_origin,
    const url::Origin& embedding_origin,
    const WebBluetoothDeviceId& device_id) {
  NOTIMPLEMENTED();
  return false;
}

bool BluetoothChooserContext::IsAllowedToAccessService(
    const url::Origin& requesting_origin,
    const url::Origin& embedding_origin,
    const WebBluetoothDeviceId& device_id,
    BluetoothUUID service) {
  NOTIMPLEMENTED();
  return false;
}

bool BluetoothChooserContext::IsValidObject(const base::Value& object) {
  NOTIMPLEMENTED();
  return false;
}

std::vector<std::unique_ptr<ChooserContextBase::Object>>
BluetoothChooserContext::GetGrantedObjects(
    const url::Origin& requesting_origin,
    const url::Origin& embedding_origin) {
  NOTIMPLEMENTED();
  return {};
}

std::vector<std::unique_ptr<ChooserContextBase::Object>>
BluetoothChooserContext::GetAllGrantedObjects() {
  NOTIMPLEMENTED();
  return {};
}

void BluetoothChooserContext::RevokeObjectPermission(
    const url::Origin& requesting_origin,
    const url::Origin& embedding_origin,
    const base::Value& object) {
  NOTIMPLEMENTED();
}
