// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/services/sharing/nearby/platform_v2/bluetooth_adapter.h"

namespace location {
namespace nearby {
namespace chrome {

BluetoothAdapter::BluetoothAdapter(bluetooth::mojom::Adapter* adapter)
    : adapter_(adapter) {}

BluetoothAdapter::~BluetoothAdapter() = default;

bool BluetoothAdapter::SetStatus(Status status) {
  // TODO(b/154848416): Implement this method.
  NOTIMPLEMENTED();
  return true;
}

bool BluetoothAdapter::IsEnabled() const {
  bluetooth::mojom::AdapterInfoPtr info;
  bool success = adapter_->GetInfo(&info);
  return success && info->present && info->powered;
}

BluetoothAdapter::ScanMode BluetoothAdapter::GetScanMode() const {
  bluetooth::mojom::AdapterInfoPtr info;
  bool success = adapter_->GetInfo(&info);

  if (!success || !info->present)
    return ScanMode::kUnknown;
  else if (!info->powered)
    return ScanMode::kNone;
  else if (!info->discoverable)
    return ScanMode::kConnectable;

  return ScanMode::kConnectableDiscoverable;
}

bool BluetoothAdapter::SetScanMode(BluetoothAdapter::ScanMode scan_mode) {
  // TODO(b/154848416): Add SetDiscoverable call to bluetooth::mojom::Adapter
  // and invoke it.
  NOTIMPLEMENTED();
  return false;
}

std::string BluetoothAdapter::GetName() const {
  bluetooth::mojom::AdapterInfoPtr info;
  bool success = adapter_->GetInfo(&info);
  return success ? info->name : "";
}

bool BluetoothAdapter::SetName(absl::string_view name) {
  // TODO(b/154848416): Add SetName call to bluetooth::mojom::Adapter and
  // invoke it.
  NOTIMPLEMENTED();
  return false;
}

}  // namespace chrome
}  // namespace nearby
}  // namespace location
