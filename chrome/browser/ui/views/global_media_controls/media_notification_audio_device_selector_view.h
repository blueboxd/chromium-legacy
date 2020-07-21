// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_GLOBAL_MEDIA_CONTROLS_MEDIA_NOTIFICATION_AUDIO_DEVICE_SELECTOR_VIEW_H_
#define CHROME_BROWSER_UI_VIEWS_GLOBAL_MEDIA_CONTROLS_MEDIA_NOTIFICATION_AUDIO_DEVICE_SELECTOR_VIEW_H_

#include "chrome/browser/ui/global_media_controls/media_notification_device_provider.h"
#include "media/audio/audio_device_description.h"
#include "ui/views/controls/button/image_button.h"
#include "ui/views/layout/box_layout.h"

class MediaNotificationContainerImplView;
class MediaNotificationService;

class MediaNotificationAudioDeviceSelectorView : public views::View,
                                                 public views::ButtonListener {
 public:
  MediaNotificationAudioDeviceSelectorView(
      MediaNotificationContainerImplView* container,
      MediaNotificationService* service,
      gfx::Size size);
  MediaNotificationAudioDeviceSelectorView(
      const MediaNotificationAudioDeviceSelectorView&) = delete;
  MediaNotificationAudioDeviceSelectorView& operator=(
      const MediaNotificationAudioDeviceSelectorView&) = delete;
  ~MediaNotificationAudioDeviceSelectorView() override;

  // Called when audio output devices are discovered.
  void UpdateAvailableAudioDevices(
      const media::AudioDeviceDescriptions& device_descriptions);

  // views::ButtonListener
  void ButtonPressed(views::Button* sender, const ui::Event& event) override;

 private:
  FRIEND_TEST_ALL_PREFIXES(MediaNotificationAudioDeviceSelectorViewTest,
                           DeviceButtonsCreated);

  void CreateDeviceButton(
      const media::AudioDeviceDescription& device_description);

  // The parent container
  MediaNotificationContainerImplView* const container_;

  MediaNotificationService* const service_;

  std::unique_ptr<MediaNotificationDeviceProvider::
                      GetOutputDevicesCallbackList::Subscription>
      audio_device_subscription_;

  // Subviews
  views::View* device_button_container_ = nullptr;

  views::View* expand_button_container_ = nullptr;
  views::ToggleImageButton* expand_button_ = nullptr;

  base::WeakPtrFactory<MediaNotificationAudioDeviceSelectorView>
      weak_ptr_factory_{this};
};

#endif  // CHROME_BROWSER_UI_VIEWS_GLOBAL_MEDIA_CONTROLS_MEDIA_NOTIFICATION_AUDIO_DEVICE_SELECTOR_VIEW_H_
