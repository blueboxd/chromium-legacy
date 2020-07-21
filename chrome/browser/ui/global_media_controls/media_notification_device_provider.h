// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_GLOBAL_MEDIA_CONTROLS_MEDIA_NOTIFICATION_DEVICE_PROVIDER_H_
#define CHROME_BROWSER_UI_GLOBAL_MEDIA_CONTROLS_MEDIA_NOTIFICATION_DEVICE_PROVIDER_H_

#include "base/callback_list.h"
#include "media/audio/audio_system.h"

class MediaNotificationDeviceProvider {
 public:
  virtual ~MediaNotificationDeviceProvider() = default;

  using GetOutputDevicesCallbackList =
      base::OnceCallbackList<void(const media::AudioDeviceDescriptions&)>;
  using GetOutputDevicesCallback = GetOutputDevicesCallbackList::CallbackType;

  virtual std::unique_ptr<MediaNotificationDeviceProvider::
                              GetOutputDevicesCallbackList::Subscription>
  GetOutputDeviceDescriptions(GetOutputDevicesCallback cb) = 0;
};

#endif  // CHROME_BROWSER_UI_GLOBAL_MEDIA_CONTROLS_MEDIA_NOTIFICATION_DEVICE_PROVIDER_H_
