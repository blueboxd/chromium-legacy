// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_HID_HID_SERVICE_H_
#define CONTENT_BROWSER_HID_HID_SERVICE_H_

#include <memory>
#include <string>
#include <vector>

#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "content/public/browser/frame_service_base.h"
#include "mojo/public/cpp/bindings/binding_set.h"
#include "services/device/public/mojom/hid.mojom.h"
#include "third_party/blink/public/mojom/hid/hid.mojom.h"

namespace content {

class HidChooser;
class RenderFrameHost;

// HidService provides an implementation of the HidService mojom interface. This
// interface is used by Blink to implement the WebHID API.
class HidService : public content::FrameServiceBase<blink::mojom::HidService> {
 public:
  static void Create(RenderFrameHost*, blink::mojom::HidServiceRequest);

  // blink::mojom::HidService:
  void GetDevices(GetDevicesCallback callback) override;
  void RequestDevice(std::vector<blink::mojom::HidDeviceFilterPtr> filters,
                     RequestDeviceCallback callback) override;
  void Connect(const std::string& device_guid,
               device::mojom::HidConnectionClientPtr client,
               ConnectCallback callback) override;

 private:
  HidService(RenderFrameHost*, blink::mojom::HidServiceRequest);
  ~HidService() override;

  void FinishGetDevices(GetDevicesCallback callback,
                        std::vector<device::mojom::HidDeviceInfoPtr> devices);
  void FinishRequestDevice(RequestDeviceCallback callback,
                           device::mojom::HidDeviceInfoPtr device);
  void FinishConnect(ConnectCallback callback,
                     device::mojom::HidConnectionPtr connection);

  // The last shown HID chooser UI.
  std::unique_ptr<HidChooser> chooser_;

  base::WeakPtrFactory<HidService> weak_factory_{this};

  DISALLOW_COPY_AND_ASSIGN(HidService);
};

}  // namespace content

#endif  // CONTENT_BROWSER_HID_HID_SERVICE_H_
