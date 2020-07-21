// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_GLOBAL_MEDIA_CONTROLS_MEDIA_NOTIFICATION_DEVICE_PROVIDER_IMPL_H_
#define CHROME_BROWSER_UI_GLOBAL_MEDIA_CONTROLS_MEDIA_NOTIFICATION_DEVICE_PROVIDER_IMPL_H_

#include "base/memory/weak_ptr.h"
#include "chrome/browser/ui/global_media_controls/media_notification_device_provider.h"

class MediaNotificationDeviceProviderImpl
    : public MediaNotificationDeviceProvider {
 public:
  MediaNotificationDeviceProviderImpl();
  MediaNotificationDeviceProviderImpl(
      const MediaNotificationDeviceProviderImpl&) = delete;
  MediaNotificationDeviceProviderImpl& operator=(
      const MediaNotificationDeviceProviderImpl&) = delete;
  ~MediaNotificationDeviceProviderImpl() override;

  std::unique_ptr<MediaNotificationDeviceProvider::
                      GetOutputDevicesCallbackList::Subscription>
  GetOutputDeviceDescriptions(
      GetOutputDevicesCallback on_descriptions_cb) override;

 private:
  void OnReceivedDeviceDescriptions(
      media::AudioDeviceDescriptions descriptions);

  bool is_querying_for_output_devices_ = false;
  MediaNotificationDeviceProvider::GetOutputDevicesCallbackList
      output_device_callback_list_;
  std::unique_ptr<media::AudioSystem> audio_system_;

  base::WeakPtrFactory<MediaNotificationDeviceProviderImpl> weak_ptr_factory_{
      this};
};

#endif  // CHROME_BROWSER_UI_GLOBAL_MEDIA_CONTROLS_MEDIA_NOTIFICATION_DEVICE_PROVIDER_IMPL_H_
