// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_GLOBAL_MEDIA_CONTROLS_MEDIA_NOTIFICATION_AUDIO_DEVICE_SELECTOR_VIEW_H_
#define CHROME_BROWSER_UI_VIEWS_GLOBAL_MEDIA_CONTROLS_MEDIA_NOTIFICATION_AUDIO_DEVICE_SELECTOR_VIEW_H_

#include "chrome/browser/ui/global_media_controls/media_notification_device_provider.h"
#include "media/audio/audio_device_description.h"
#include "ui/views/controls/button/image_button.h"
#include "ui/views/controls/button/md_text_button.h"
#include "ui/views/layout/box_layout.h"

namespace views {
class Button;
}  // namespace views

class MediaNotificationAudioDeviceSelectorViewDelegate;
class MediaNotificationService;

class MediaNotificationAudioDeviceSelectorView : public views::View,
                                                 public views::ButtonListener {
 public:
  MediaNotificationAudioDeviceSelectorView(
      MediaNotificationAudioDeviceSelectorViewDelegate* delegate,
      MediaNotificationService* service,
      gfx::Size size,
      const std::string& current_device_id);
  MediaNotificationAudioDeviceSelectorView(
      const MediaNotificationAudioDeviceSelectorView&) = delete;
  MediaNotificationAudioDeviceSelectorView& operator=(
      const MediaNotificationAudioDeviceSelectorView&) = delete;
  ~MediaNotificationAudioDeviceSelectorView() override;

  // Called when audio output devices are discovered.
  void UpdateAvailableAudioDevices(
      const media::AudioDeviceDescriptions& device_descriptions);

  // Called when an audio device switch has occurred
  void UpdateCurrentAudioDevice(std::string current_device_id);

  // views::ButtonListener
  void ButtonPressed(views::Button* sender, const ui::Event& event) override;

 private:
  FRIEND_TEST_ALL_PREFIXES(MediaNotificationAudioDeviceSelectorViewTest,
                           DeviceButtonsCreated);
  FRIEND_TEST_ALL_PREFIXES(MediaNotificationAudioDeviceSelectorViewTest,
                           DeviceButtonClickNotifiesContainer);
  FRIEND_TEST_ALL_PREFIXES(MediaNotificationAudioDeviceSelectorViewTest,
                           CurrentDeviceHighlighted);
  FRIEND_TEST_ALL_PREFIXES(MediaNotificationAudioDeviceSelectorViewTest,
                           DeviceHighlightedOnChange);
  FRIEND_TEST_ALL_PREFIXES(MediaNotificationAudioDeviceSelectorViewTest,
                           DeviceButtonsChange);

  void CreateDeviceButton(
      const media::AudioDeviceDescription& device_description);

  bool ShouldBeVisible(
      const media::AudioDeviceDescriptions& device_descriptions);

  MediaNotificationAudioDeviceSelectorViewDelegate* const delegate_;
  MediaNotificationService* const service_;

  std::unique_ptr<MediaNotificationDeviceProvider::
                      GetOutputDevicesCallbackList::Subscription>
      audio_device_subscription_;

  // Subviews
  views::View* device_button_container_ = nullptr;

  views::View* expand_button_container_ = nullptr;
  views::ToggleImageButton* expand_button_ = nullptr;

  views::MdTextButton* current_device_button_ = nullptr;
  std::string current_device_id_;

  // Maps button pointers to the string ID of the audio sink they represent.
  std::map<views::Button*, std::string> sink_id_map_;

  base::WeakPtrFactory<MediaNotificationAudioDeviceSelectorView>
      weak_ptr_factory_{this};
};

#endif  // CHROME_BROWSER_UI_VIEWS_GLOBAL_MEDIA_CONTROLS_MEDIA_NOTIFICATION_AUDIO_DEVICE_SELECTOR_VIEW_H_
