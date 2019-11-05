// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/sharing/sms/sms_remote_fetcher.h"

#include "base/logging.h"
#include "chrome/browser/sharing/sharing_constants.h"
#include "chrome/browser/sharing/sharing_service.h"
#include "chrome/browser/sharing/sharing_service_factory.h"
#include "chrome/browser/sharing/sms/sms_flags.h"
#include "components/sync_device_info/device_info.h"
#include "content/public/browser/browser_context.h"

void FetchRemoteSms(
    content::BrowserContext* context,
    const url::Origin& origin,
    base::OnceCallback<void(base::Optional<std::string>)> callback) {
  if (!base::FeatureList::IsEnabled(kSmsFetchRequestHandler)) {
    return;
  }

  SharingService* sharing_service =
      SharingServiceFactory::GetForBrowserContext(context);
  SharingService::SharingDeviceList devices =
      sharing_service->GetDeviceCandidates(
          sync_pb::SharingSpecificFields::SMS_FETCHER);
  for (const std::unique_ptr<syncer::DeviceInfo>& info : devices) {
    chrome_browser_sharing::SharingMessage sharing_message;

    sharing_service->SendMessageToDevice(
        info->guid(), kSendMessageTimeout, std::move(sharing_message),
        base::BindOnce(
            [](SharingSendMessageResult result,
               std::unique_ptr<chrome_browser_sharing::ResponseMessage>
                   response) {
              // TODO(crbug.com/1015645): implementation pending.
              NOTIMPLEMENTED();
            }));

    // Sends to the first device that has the capability enabled.
    // TODO(crbug.com/1015645): figure out the routing strategy.
    break;
  }
}
