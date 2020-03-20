// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/bluetooth/chrome_bluetooth_delegate.h"

#include <memory>

#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/bluetooth/bluetooth_chooser_context.h"
#include "chrome/browser/bluetooth/bluetooth_chooser_context_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "content/public/browser/web_contents.h"
#include "device/bluetooth/bluetooth_device.h"
#include "device/bluetooth/public/cpp/bluetooth_uuid.h"
#include "third_party/blink/public/common/bluetooth/web_bluetooth_device_id.h"
#include "third_party/blink/public/mojom/bluetooth/web_bluetooth.mojom.h"

using blink::WebBluetoothDeviceId;
using content::RenderFrameHost;
using content::WebContents;
using device::BluetoothUUID;

namespace {

BluetoothChooserContext* GetBluetoothChooserContext(WebContents* web_contents) {
  auto* profile =
      Profile::FromBrowserContext(web_contents->GetBrowserContext());
  return BluetoothChooserContextFactory::GetForProfile(profile);
}

}  // namespace

ChromeBluetoothDelegate::ChromeBluetoothDelegate() = default;

ChromeBluetoothDelegate::~ChromeBluetoothDelegate() = default;

WebBluetoothDeviceId ChromeBluetoothDelegate::GetWebBluetoothDeviceId(
    RenderFrameHost* frame,
    const std::string& device_address) {
  auto* web_contents = WebContents::FromRenderFrameHost(frame);
  return GetBluetoothChooserContext(web_contents)
      ->GetWebBluetoothDeviceId(
          frame->GetLastCommittedOrigin(),
          web_contents->GetMainFrame()->GetLastCommittedOrigin(),
          device_address);
}

std::string ChromeBluetoothDelegate::GetDeviceAddress(
    RenderFrameHost* frame,
    const WebBluetoothDeviceId& device_id) {
  auto* web_contents = WebContents::FromRenderFrameHost(frame);
  return GetBluetoothChooserContext(web_contents)
      ->GetDeviceAddress(frame->GetLastCommittedOrigin(),
                         web_contents->GetMainFrame()->GetLastCommittedOrigin(),
                         device_id);
}

WebBluetoothDeviceId ChromeBluetoothDelegate::AddScannedDevice(
    RenderFrameHost* frame,
    const std::string& device_address) {
  auto* web_contents = WebContents::FromRenderFrameHost(frame);
  return GetBluetoothChooserContext(web_contents)
      ->AddScannedDevice(frame->GetLastCommittedOrigin(),
                         web_contents->GetMainFrame()->GetLastCommittedOrigin(),
                         device_address);
}

WebBluetoothDeviceId ChromeBluetoothDelegate::GrantServiceAccessPermission(
    RenderFrameHost* frame,
    const device::BluetoothDevice* device,
    const blink::mojom::WebBluetoothRequestDeviceOptions* options) {
  auto* web_contents = WebContents::FromRenderFrameHost(frame);
  return GetBluetoothChooserContext(web_contents)
      ->GrantServiceAccessPermission(
          frame->GetLastCommittedOrigin(),
          web_contents->GetMainFrame()->GetLastCommittedOrigin(), device,
          options);
}

bool ChromeBluetoothDelegate::HasDevicePermission(
    RenderFrameHost* frame,
    const WebBluetoothDeviceId& device_id) {
  auto* web_contents = WebContents::FromRenderFrameHost(frame);
  return GetBluetoothChooserContext(web_contents)
      ->HasDevicePermission(
          frame->GetLastCommittedOrigin(),
          web_contents->GetMainFrame()->GetLastCommittedOrigin(), device_id);
}

bool ChromeBluetoothDelegate::IsAllowedToAccessService(
    RenderFrameHost* frame,
    const WebBluetoothDeviceId& device_id,
    const BluetoothUUID& service) {
  auto* web_contents = WebContents::FromRenderFrameHost(frame);
  return GetBluetoothChooserContext(web_contents)
      ->IsAllowedToAccessService(
          frame->GetLastCommittedOrigin(),
          web_contents->GetMainFrame()->GetLastCommittedOrigin(), device_id,
          service);
}

bool ChromeBluetoothDelegate::IsAllowedToAccessAtLeastOneService(
    RenderFrameHost* frame,
    const WebBluetoothDeviceId& device_id) {
  auto* web_contents = WebContents::FromRenderFrameHost(frame);
  return GetBluetoothChooserContext(web_contents)
      ->IsAllowedToAccessAtLeastOneService(
          frame->GetLastCommittedOrigin(),
          web_contents->GetMainFrame()->GetLastCommittedOrigin(), device_id);
}

std::vector<blink::mojom::WebBluetoothDevicePtr>
ChromeBluetoothDelegate::GetPermittedDevices(content::RenderFrameHost* frame) {
  auto* web_contents = WebContents::FromRenderFrameHost(frame);
  auto* context = GetBluetoothChooserContext(web_contents);
  std::vector<std::unique_ptr<permissions::ChooserContextBase::Object>>
      objects = context->GetGrantedObjects(
          frame->GetLastCommittedOrigin(),
          web_contents->GetMainFrame()->GetLastCommittedOrigin());
  std::vector<blink::mojom::WebBluetoothDevicePtr> permitted_devices;

  for (const auto& object : objects) {
    auto permitted_device = blink::mojom::WebBluetoothDevice::New();
    permitted_device->id =
        BluetoothChooserContext::GetObjectDeviceId(object->value);
    permitted_device->name =
        base::UTF16ToUTF8(context->GetObjectDisplayName(object->value));
    permitted_devices.push_back(std::move(permitted_device));
  }

  return permitted_devices;
}
