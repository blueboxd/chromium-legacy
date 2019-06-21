// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/usb_internals/usb_internals_page_handler.h"

#include <utility>

#include "content/public/browser/system_connector.h"
#include "services/device/public/mojom/constants.mojom.h"
#include "services/service_manager/public/cpp/connector.h"

UsbInternalsPageHandler::UsbInternalsPageHandler(
    mojom::UsbInternalsPageHandlerRequest request)
    : binding_(this, std::move(request)) {}

UsbInternalsPageHandler::~UsbInternalsPageHandler() {}

void UsbInternalsPageHandler::BindTestInterface(
    device::mojom::UsbDeviceManagerTestRequest request) {
  // Forward the request to the DeviceService.
  content::GetSystemConnector()->BindInterface(device::mojom::kServiceName,
                                               std::move(request));
}

void UsbInternalsPageHandler::BindUsbDeviceManagerInterface(
    device::mojom::UsbDeviceManagerRequest request) {
  // Forward the request to the DeviceService.
  content::GetSystemConnector()->BindInterface(device::mojom::kServiceName,
                                               std::move(request));
}
