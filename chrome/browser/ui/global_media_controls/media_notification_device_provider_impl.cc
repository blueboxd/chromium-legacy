// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/global_media_controls/media_notification_device_provider_impl.h"

#include "content/public/browser/audio_service.h"

MediaNotificationDeviceProviderImpl::MediaNotificationDeviceProviderImpl() =
    default;
MediaNotificationDeviceProviderImpl::~MediaNotificationDeviceProviderImpl() =
    default;

std::unique_ptr<
    MediaNotificationDeviceProvider::GetOutputDevicesCallbackList::Subscription>
MediaNotificationDeviceProviderImpl::GetOutputDeviceDescriptions(
    GetOutputDevicesCallback cb) {
  if (is_querying_for_output_devices_)
    return output_device_callback_list_.Add(std::move(cb));

  if (!audio_system_)
    audio_system_ = content::CreateAudioSystemForAudioService();
  audio_system_->GetDeviceDescriptions(
      /*for_input=*/false,
      base::BindOnce(
          &MediaNotificationDeviceProviderImpl::OnReceivedDeviceDescriptions,
          weak_ptr_factory_.GetWeakPtr()));
  return output_device_callback_list_.Add(std::move(cb));
}

void MediaNotificationDeviceProviderImpl::OnReceivedDeviceDescriptions(
    media::AudioDeviceDescriptions descriptions) {
  is_querying_for_output_devices_ = false;
  output_device_callback_list_.Notify(descriptions);
}
