// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_BLUETOOTH_BLUETOOTH_CHOOSER_CONTEXT_H_
#define CHROME_BROWSER_BLUETOOTH_BLUETOOTH_CHOOSER_CONTEXT_H_

#include <string>

#include "base/containers/flat_set.h"
#include "chrome/browser/permissions/chooser_context_base.h"
#include "device/bluetooth/public/cpp/bluetooth_uuid.h"
#include "third_party/blink/public/common/bluetooth/web_bluetooth_device_id.h"

namespace base {
class Value;
}  // namespace base

namespace url {
class Origin;
}  // namespace url

// Manages the permissions for Web Bluetooth device objects. A Web Bluetooth
// permission object consists of its WebBluetoothDeviceId and set of Bluetooth
// service UUIDs. The WebBluetoothDeviceId is generated randomly by this class
// and is unique for a given Bluetooth device address and origin pair, so this
// class stores this mapping and provides utility methods to convert between
// the WebBluetoothDeviceId and Bluetooth device address.
class BluetoothChooserContext : public ChooserContextBase {
 public:
  explicit BluetoothChooserContext(Profile* profile);
  ~BluetoothChooserContext() override;

  // Set class as move-only.
  BluetoothChooserContext(const BluetoothChooserContext&) = delete;
  BluetoothChooserContext& operator=(const BluetoothChooserContext&) = delete;

  // Helper methods for converting between a WebBluetoothDeviceId and a
  // Bluetooth device address string for a given origin pair.
  const blink::WebBluetoothDeviceId GetWebBluetoothDeviceId(
      const url::Origin& requesting_origin,
      const url::Origin& embedding_origin,
      const std::string& device_address);
  const std::string GetDeviceAddress(
      const url::Origin& requesting_origin,
      const url::Origin& embedding_origin,
      const blink::WebBluetoothDeviceId& device_id);

  // Bluetooth-specific interface for granting and checking permissions.
  const blink::WebBluetoothDeviceId GrantDevicePermission(
      const url::Origin& requesting_origin,
      const url::Origin& embedding_origin,
      const std::string& device_address,
      base::flat_set<device::BluetoothUUID, device::BluetoothUUIDHash>&
          services);
  bool HasDevicePermission(const url::Origin& requesting_origin,
                           const url::Origin& embedding_origin,
                           const blink::WebBluetoothDeviceId& device_id);
  bool IsAllowedToAccessAtLeastOneService(
      const url::Origin& requesting_origin,
      const url::Origin& embedding_origin,
      const blink::WebBluetoothDeviceId& device_id);
  bool IsAllowedToAccessService(const url::Origin& requesting_origin,
                                const url::Origin& embedding_origin,
                                const blink::WebBluetoothDeviceId& device_id,
                                device::BluetoothUUID service);

 protected:
  // ChooserContextBase implementation;
  bool IsValidObject(const base::Value& object) override;
  std::vector<std::unique_ptr<Object>> GetGrantedObjects(
      const url::Origin& requesting_origin,
      const url::Origin& embedding_origin) override;
  std::vector<std::unique_ptr<Object>> GetAllGrantedObjects() override;
  void RevokeObjectPermission(const url::Origin& requesting_origin,
                              const url::Origin& embedding_origin,
                              const base::Value& object) override;
};

#endif  // CHROME_BROWSER_BLUETOOTH_BLUETOOTH_CHOOSER_CONTEXT_H_
