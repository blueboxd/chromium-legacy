// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/global_media_controls/media_notification_device_provider_impl.h"

#include "base/bind.h"
#include "chrome/browser/ui/global_media_controls/media_notification_device_monitor.h"
#include "content/public/browser/audio_service.h"

MediaNotificationDeviceProviderImpl::MediaNotificationDeviceProviderImpl()
    : monitor_(MediaNotificationDeviceMonitor::Create(this)) {
  monitor_->AddDevicesChangedObserver(this);
  output_device_callback_list_.set_removal_callback(base::BindRepeating(
      &MediaNotificationDeviceProviderImpl::OnSubscriberRemoved,
      weak_ptr_factory_.GetWeakPtr()));
}

MediaNotificationDeviceProviderImpl::~MediaNotificationDeviceProviderImpl() {
  monitor_->RemoveDevicesChangedObserver(this);
}

std::unique_ptr<
    MediaNotificationDeviceProvider::GetOutputDevicesCallbackList::Subscription>
MediaNotificationDeviceProviderImpl::RegisterOutputDeviceDescriptionsCallback(
    GetOutputDevicesCallback cb) {
  monitor_->StartMonitoring();
  if (has_device_list_)
    cb.Run(audio_device_descriptions_);
  auto subscription = output_device_callback_list_.Add(std::move(cb));
  if (!has_device_list_)
    GetDevices();
  return subscription;
}

void MediaNotificationDeviceProviderImpl::GetOutputDeviceDescriptions(
    media::AudioSystem::OnDeviceDescriptionsCallback cb) {
  if (!audio_system_)
    audio_system_ = content::CreateAudioSystemForAudioService();
  audio_system_->GetDeviceDescriptions(
      /*for_input=*/false, std::move(cb));
}

void MediaNotificationDeviceProviderImpl::OnDevicesChanged() {
  GetDevices();
}

void MediaNotificationDeviceProviderImpl::GetDevices() {
  if (is_querying_for_output_devices_)
    return;
  is_querying_for_output_devices_ = true;
  GetOutputDeviceDescriptions(
      base::BindOnce(&MediaNotificationDeviceProviderImpl::NotifySubscribers,
                     weak_ptr_factory_.GetWeakPtr()));
}

void MediaNotificationDeviceProviderImpl::NotifySubscribers(
    media::AudioDeviceDescriptions descriptions) {
  is_querying_for_output_devices_ = false;
  audio_device_descriptions_ = std::move(descriptions);
  has_device_list_ = true;
  output_device_callback_list_.Notify(audio_device_descriptions_);
}

void MediaNotificationDeviceProviderImpl::OnSubscriberRemoved() {
  if (output_device_callback_list_.empty())
    monitor_->StopMonitoring();
}
