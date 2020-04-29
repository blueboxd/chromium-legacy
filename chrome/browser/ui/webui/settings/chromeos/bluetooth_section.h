// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_SETTINGS_CHROMEOS_BLUETOOTH_SECTION_H_
#define CHROME_BROWSER_UI_WEBUI_SETTINGS_CHROMEOS_BLUETOOTH_SECTION_H_

#include "base/memory/ref_counted.h"
#include "base/memory/weak_ptr.h"
#include "chrome/browser/ui/webui/settings/chromeos/os_settings_section.h"
#include "device/bluetooth/bluetooth_adapter.h"

namespace content {
class WebUIDataSource;
}  // namespace content

namespace chromeos {
namespace settings {

// Provides UI strings and search tags for Bluetooth settings. Different search
// tags are registered depending on whether the device has a Bluetooth chip and
// whether it is turned on or off.
class BluetoothSection : public OsSettingsSection,
                         public device::BluetoothAdapter::Observer {
 public:
  BluetoothSection(Profile* profile, Delegate* per_page_delegate);
  ~BluetoothSection() override;

 private:
  // OsSettingsSection:
  void AddLoadTimeData(content::WebUIDataSource* html_source) override;

  // device::BluetoothAdapter::Observer:
  void AdapterPresentChanged(device::BluetoothAdapter* adapter,
                             bool present) override;
  void AdapterPoweredChanged(device::BluetoothAdapter* adapter,
                             bool powered) override;

  void OnFetchBluetoothAdapter(
      scoped_refptr<device::BluetoothAdapter> bluetooth_adapter);
  void UpdateSearchTags();

  scoped_refptr<device::BluetoothAdapter> bluetooth_adapter_;
  base::WeakPtrFactory<BluetoothSection> weak_ptr_factory_{this};
};

}  // namespace settings
}  // namespace chromeos

#endif  // CHROME_BROWSER_UI_WEBUI_SETTINGS_CHROMEOS_BLUETOOTH_SECTION_H_
