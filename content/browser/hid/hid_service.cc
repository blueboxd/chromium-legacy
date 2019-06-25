// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/hid/hid_service.h"

#include <utility>

#include "base/bind.h"
#include "base/callback.h"
#include "content/public/browser/content_browser_client.h"
#include "content/public/browser/hid_chooser.h"
#include "content/public/browser/hid_delegate.h"
#include "content/public/browser/render_frame_host.h"
#include "mojo/public/cpp/bindings/interface_request.h"
#include "mojo/public/cpp/bindings/message.h"
#include "third_party/blink/public/mojom/feature_policy/feature_policy.mojom.h"

namespace content {

HidService::HidService(RenderFrameHost* render_frame_host,
                       blink::mojom::HidServiceRequest request)
    : FrameServiceBase(render_frame_host, std::move(request)) {}

HidService::~HidService() = default;

// static
void HidService::Create(RenderFrameHost* render_frame_host,
                        blink::mojom::HidServiceRequest request) {
  DCHECK(render_frame_host);

  if (!render_frame_host->IsFeatureEnabled(
          blink::mojom::FeaturePolicyFeature::kHid)) {
    mojo::ReportBadMessage("Feature policy blocks access to HID.");
    return;
  }

  // Avoid creating the HidService if there is no HID delegate to provide the
  // implementation.
  if (!GetContentClient()->browser()->GetHidDelegate())
    return;

  // HidService owns itself. It will self-destruct when a mojo interface error
  // occurs, the render frame host is deleted, or the render frame host
  // navigates to a new document.
  new HidService(render_frame_host, std::move(request));
}

void HidService::GetDevices(GetDevicesCallback callback) {
  GetContentClient()
      ->browser()
      ->GetHidDelegate()
      ->GetHidManager(web_contents())
      ->GetDevices(base::BindOnce(&HidService::FinishGetDevices,
                                  weak_factory_.GetWeakPtr(),
                                  std::move(callback)));
}

void HidService::RequestDevice(
    std::vector<blink::mojom::HidDeviceFilterPtr> filters,
    RequestDeviceCallback callback) {
  HidDelegate* delegate = GetContentClient()->browser()->GetHidDelegate();
  if (!delegate->CanRequestDevicePermission(web_contents(), origin())) {
    std::move(callback).Run(nullptr);
    return;
  }

  chooser_ = delegate->RunChooser(
      render_frame_host(), std::move(filters),
      base::BindOnce(&HidService::FinishRequestDevice,
                     weak_factory_.GetWeakPtr(), std::move(callback)));
}

void HidService::Connect(const std::string& device_guid,
                         device::mojom::HidConnectionClientPtr client,
                         ConnectCallback callback) {
  GetContentClient()
      ->browser()
      ->GetHidDelegate()
      ->GetHidManager(web_contents())
      ->Connect(
          device_guid, std::move(client),
          base::BindOnce(&HidService::FinishConnect, weak_factory_.GetWeakPtr(),
                         std::move(callback)));
}

void HidService::FinishGetDevices(
    GetDevicesCallback callback,
    std::vector<device::mojom::HidDeviceInfoPtr> devices) {
  std::vector<device::mojom::HidDeviceInfoPtr> result;
  HidDelegate* delegate = GetContentClient()->browser()->GetHidDelegate();
  for (auto& device : devices) {
    if (delegate->HasDevicePermission(web_contents(), origin(), *device))
      result.push_back(std::move(device));
  }

  std::move(callback).Run(std::move(result));
}

void HidService::FinishRequestDevice(RequestDeviceCallback callback,
                                     device::mojom::HidDeviceInfoPtr device) {
  if (!device) {
    std::move(callback).Run(nullptr);
    return;
  }

  std::move(callback).Run(std::move(device));
}

void HidService::FinishConnect(ConnectCallback callback,
                               device::mojom::HidConnectionPtr connection) {
  if (!connection) {
    std::move(callback).Run(nullptr);
    return;
  }

  std::move(callback).Run(std::move(connection));
}

}  // namespace content
